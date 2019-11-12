// MythTV
#include "mythcorecontext.h"
#include "mythegldefs.h"
#include "videocolourspace.h"
#include "fourcc.h"
#include "mythvaapidrminterop.h"

// DRM PRIME interop largely for testing
#include "mythdrmprimeinterop.h"
extern "C" {
#include "libavutil/hwcontext_drm.h"
}
#include <unistd.h>

#define LOC QString("VAAPIDRM: ")

MythVAAPIInteropDRM::MythVAAPIInteropDRM(MythRenderOpenGL *Context)
  : MythVAAPIInterop(Context, VAAPIEGLDRM)
{
    QString device = gCoreContext->GetSetting("VAAPIDevice");
    if (device.isEmpty())
        device = "/dev/dri/renderD128";
    m_drmFile.setFileName(device);
    if (m_drmFile.open(QIODevice::ReadWrite))
    {
        m_vaDisplay = vaGetDisplayDRM(m_drmFile.handle());
        if (!m_vaDisplay)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create DRM VADisplay");
            return;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open %1").arg(device));
        return;
    }
    InitaliseDisplay();

    if (!qgetenv("MYTHTV_VAAPI_PRIME").isEmpty())
        m_drmPrimeInterop = new MythDRMPRIMEInterop(Context, false);
}

MythVAAPIInteropDRM::~MythVAAPIInteropDRM()
{
    OpenGLLocker locker(m_context);

    CleanupDRMPRIME();

    CleanupReferenceFrames();
    DestroyDeinterlacer();
    DeleteTextures();

    if (m_drmFile.isOpen())
        m_drmFile.close();
}

void MythVAAPIInteropDRM::DeleteTextures(void)
{
    OpenGLLocker locker(m_context);

    if (!m_openglTextures.isEmpty() && m_context->IsEGL())
    {
        int count = 0;
        QHash<unsigned long long, vector<MythVideoTexture*> >::const_iterator it = m_openglTextures.constBegin();
        for ( ; it != m_openglTextures.constEnd(); ++it)
        {
            vector<MythVideoTexture*> textures = it.value();
            vector<MythVideoTexture*>::iterator it2 = textures.begin();
            for ( ; it2 != textures.end(); ++it2)
            {
                if ((*it2)->m_data)
                {
                    m_context->eglDestroyImageKHR(m_context->GetEGLDisplay(), (*it2)->m_data);
                    (*it2)->m_data = nullptr;
                    count++;
                }
            }
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Deleted %1 EGL images in %2 groups")
            .arg(count).arg(m_openglTextures.size()));
    }

    MythVAAPIInterop::DeleteTextures();
}

void MythVAAPIInteropDRM::DestroyDeinterlacer(void)
{
    if (m_filterGraph)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting deinterlacer frame cache");
        DeleteTextures();
    }
    MythVAAPIInterop::DestroyDeinterlacer();
}

void MythVAAPIInteropDRM::PostInitDeinterlacer(void)
{
    // remove the old, non-deinterlaced frame cache
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting progressive frame cache");
    DeleteTextures();
}

void MythVAAPIInteropDRM::CleanupReferenceFrames(void)
{
    while (!m_referenceFrames.isEmpty())
    {
        AVBufferRef* ref = m_referenceFrames.takeLast();
        av_buffer_unref(&ref);
    }
}

void MythVAAPIInteropDRM::RotateReferenceFrames(AVBufferRef *Buffer)
{
    if (!Buffer)
        return;

    // don't retain twice for double rate
    if ((m_referenceFrames.size() > 0) &&
            (static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[0]->data)) ==
             static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(Buffer->data))))
    {
        return;
    }

    m_referenceFrames.push_front(av_buffer_ref(Buffer));

    // release old frames
    while (m_referenceFrames.size() > 3)
    {
        AVBufferRef* ref = m_referenceFrames.takeLast();
        av_buffer_unref(&ref);
    }
}

vector<MythVideoTexture*> MythVAAPIInteropDRM::GetReferenceFrames(void)
{
    vector<MythVideoTexture*> result;
    int size = m_referenceFrames.size();
    if (size < 1)
        return result;

    VASurfaceID next = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[0]->data));
    VASurfaceID current = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[size > 1 ? 1 : 0]->data));
    VASurfaceID last = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[size > 2 ? 2 : 0]->data));

    if (!m_openglTextures.contains(next) || !m_openglTextures.contains(current) ||
        !m_openglTextures.contains(last))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Reference frame error");
        return result;
    }

    result = m_openglTextures[last];
    foreach (MythVideoTexture* tex, m_openglTextures[current])
        result.push_back(tex);
    foreach (MythVideoTexture* tex, m_openglTextures[next])
        result.push_back(tex);
    return result;
}

vector<MythVideoTexture*> MythVAAPIInteropDRM::Acquire(MythRenderOpenGL *Context,
                                                       VideoColourSpace *ColourSpace,
                                                       VideoFrame *Frame,
                                                       FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    VASurfaceID id = VerifySurface(Context, Frame);
    if (!id || !m_vaDisplay)
        return result;

    // Update frame colourspace and initialise on first frame
    if (ColourSpace)
    {
        if (m_openglTextures.isEmpty())
            ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Deinterlacing
    bool needreferenceframes = false;

    if (is_interlaced(Scan))
    {
        // allow GLSL deinterlacers
        Frame->deinterlace_allowed = Frame->deinterlace_allowed | DEINT_SHADER;

        // is GLSL preferred - and if so do we need reference frames
        bool glsldeint = false;

        // we explicitly use a shader if preferred over driver. If CPU only
        // is preferred, the default will be to use the driver instead and if that
        // fails we fall back to GLSL
        MythDeintType shader = GetDoubleRateOption(Frame, DEINT_SHADER);
        MythDeintType driver = GetDoubleRateOption(Frame, DEINT_DRIVER);
        if (m_filterError)
            shader = GetDoubleRateOption(Frame, DEINT_SHADER | DEINT_CPU | DEINT_DRIVER, DEINT_ALL);
        if (shader && !driver)
        {
            glsldeint = true;
            needreferenceframes = shader == DEINT_HIGH;
            Frame->deinterlace_double = Frame->deinterlace_double | DEINT_SHADER;
        }
        else if (!shader && !driver) // singlerate
        {
            shader = GetSingleRateOption(Frame, DEINT_SHADER);
            driver = GetSingleRateOption(Frame, DEINT_DRIVER);
            if (m_filterError)
                shader = GetSingleRateOption(Frame, DEINT_SHADER | DEINT_CPU | DEINT_DRIVER, DEINT_ALL);
            if (shader && !driver)
            {
                glsldeint = true;
                needreferenceframes = shader == DEINT_HIGH;
                Frame->deinterlace_single = Frame->deinterlace_single | DEINT_SHADER;
            }
        }

        // driver deinterlacing
        if (!glsldeint)
            id = Deinterlace(Frame, id, Scan);

        // fallback to shaders if VAAPI deints fail
        if (m_filterError)
            Frame->deinterlace_allowed = Frame->deinterlace_allowed & ~DEINT_DRIVER;
    }
    else if (m_deinterlacer)
    {
        DestroyDeinterlacer();
    }

    if (needreferenceframes)
    {
        if (abs(Frame->frameCounter - m_discontinuityCounter) > 1)
            CleanupReferenceFrames();
        RotateReferenceFrames(reinterpret_cast<AVBufferRef*>(Frame->priv[0]));
    }
    else
    {
        CleanupReferenceFrames();
    }
    m_discontinuityCounter = Frame->frameCounter;

    // return cached texture if available
    if (m_openglTextures.contains(id))
    {
        if (needreferenceframes)
            return GetReferenceFrames();
        else
            return m_openglTextures[id];
    }

    OpenGLLocker locker(m_context);

    if (m_drmPrimeInterop)
    {
        result = AcquirePrime(id, Context, ColourSpace, Frame, Scan);
    }
    else
    {
        VAImage vaimage;
        memset(&vaimage, 0, sizeof(vaimage));
        vaimage.buf = vaimage.image_id = VA_INVALID_ID;
        INIT_ST;
        va_status = vaDeriveImage(m_vaDisplay, id, &vaimage);
        CHECK_ST;
        uint count = vaimage.num_planes;

        VABufferInfo vabufferinfo;
        memset(&vabufferinfo, 0, sizeof(vabufferinfo));
        vabufferinfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
        va_status = vaAcquireBufferHandle(m_vaDisplay, vaimage.buf, &vabufferinfo);
        CHECK_ST;

        VideoFrameType format = VATypeToMythType(vaimage.format.fourcc);
        if (format == FMT_NONE)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported VA fourcc: %1")
                .arg(fourcc_str(static_cast<int32_t>(vaimage.format.fourcc))));
        }
        else
        {
            if (count != planes(format))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Inconsistent plane count %1 != %2")
                    .arg(count).arg(planes(format)));
            }
            else
            {
                vector<QSize> sizes;
                for (uint plane = 0 ; plane < count; ++plane)
                {
                    QSize size(vaimage.width, vaimage.height);
                    if (plane > 0)
                        size = QSize(vaimage.width >> 1, vaimage.height >> 1);
                    sizes.push_back(size);
                }

                vector<MythVideoTexture*> textures = MythVideoTexture::CreateTextures(m_context, FMT_VAAPI, format, sizes);
                if (textures.size() != count)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create all textures");
                }
                else
                {
                    for (uint i = 0; i < textures.size(); ++i)
                        textures[i]->m_allowGLSLDeint = true;
                    CreateDRMBuffers(format, textures, vabufferinfo.handle, vaimage);
                    result = textures;
                }
            }
        }

        va_status = vaReleaseBufferHandle(m_vaDisplay, vaimage.buf);
        CHECK_ST;
        va_status = vaDestroyImage(m_vaDisplay, vaimage.image_id);
        CHECK_ST;
    }

    m_openglTextures.insert(id, result);
    if (needreferenceframes)
        return GetReferenceFrames();
    return result;
}

VideoFrameType MythVAAPIInteropDRM::VATypeToMythType(uint32_t Fourcc)
{
    switch (Fourcc)
    {
        case VA_FOURCC_IYUV:
        case VA_FOURCC_I420: return FMT_YV12;
        case VA_FOURCC_NV12: return FMT_NV12;
        case VA_FOURCC_YUY2: return FMT_YUY2;
        case VA_FOURCC_UYVY: return FMT_YUY2; // ?
        case VA_FOURCC_P010: return FMT_P010;
        case VA_FOURCC_P016: return FMT_P016;
        case VA_FOURCC_ARGB: return FMT_ARGB32;
        case VA_FOURCC_RGBA: return FMT_RGBA32;
    }
    return FMT_NONE;
}

#ifndef DRM_FORMAT_R8
#define DRM_FORMAT_R8       MKTAG('R', '8', ' ', ' ')
#define DRM_FORMAT_GR88     MKTAG('G', 'R', '8', '8')
#define DRM_FORMAT_R16      MKTAG('R', '1', '6', ' ')
#define DRM_FORMAT_GR32     MKTAG('G', 'R', '3', '2')
#endif

/*! \brief Create a set of EGL images/DRM buffers associated with the given textures
*/
void MythVAAPIInteropDRM::CreateDRMBuffers(VideoFrameType Format,
                                           vector<MythVideoTexture*> Textures,
                                           uintptr_t Handle, VAImage &Image)
{
    for (uint plane = 0; plane < Textures.size(); ++plane)
    {
        MythVideoTexture* texture = Textures[plane];
        int fourcc = (Format == FMT_P010) ? DRM_FORMAT_R16 : DRM_FORMAT_R8;
        if (plane > 0)
            fourcc = (Format == FMT_P010) ? DRM_FORMAT_GR32 : DRM_FORMAT_GR88;
        const EGLint attributes[] = {
                EGL_LINUX_DRM_FOURCC_EXT, fourcc,
                EGL_WIDTH,  texture->m_size.width(),
                EGL_HEIGHT, texture->m_size.height(),
                EGL_DMA_BUF_PLANE0_FD_EXT,     static_cast<EGLint>(Handle),
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(Image.offsets[plane]),
                EGL_DMA_BUF_PLANE0_PITCH_EXT,  static_cast<EGLint>(Image.pitches[plane]),
                EGL_NONE
        };

        EGLImageKHR image = m_context->eglCreateImageKHR(m_context->GetEGLDisplay(), EGL_NO_CONTEXT,
                                                         EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
        if (!image)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage for plane %1 %2")
                .arg(plane).arg(m_context->GetEGLError()));

        m_context->glBindTexture(texture->m_target, texture->m_textureId);
        m_context->eglImageTargetTexture2DOES(texture->m_target, image);
        m_context->glBindTexture(texture->m_target, 0);
        texture->m_data = static_cast<unsigned char *>(image);
    }
}

bool MythVAAPIInteropDRM::IsSupported(MythRenderOpenGL *Context)
{
    if (!Context)
        return false;

    OpenGLLocker locker(Context);
    return Context->IsEGL() &&
           Context->HasEGLExtension("EGL_EXT_image_dma_buf_import") &&
           Context->hasExtension("GL_OES_EGL_image");
}

/*! \brief Export the given VideoFrame as a DRM PRIME descriptor
 *
 * This is funcionally equivalent to the 'regular' VAAPI version but is useful
 * for testing DRM PRIME functionality on desktops.
*/
vector<MythVideoTexture*> MythVAAPIInteropDRM::AcquirePrime(VASurfaceID Id,
                                                            MythRenderOpenGL *Context,
                                                            VideoColourSpace *ColourSpace,
                                                            VideoFrame *Frame,
                                                            FrameScanType Scan)
{
    vector<MythVideoTexture*> result;

    if (!m_drmFrames.contains(Id))
    {
        INIT_ST;
        uint32_t exportflags = VA_EXPORT_SURFACE_SEPARATE_LAYERS | VA_EXPORT_SURFACE_READ_ONLY;
        VADRMPRIMESurfaceDescriptor vadesc;
        va_status = vaExportSurfaceHandle(m_vaDisplay, Id,
                                          VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                          exportflags, &vadesc);
        CHECK_ST;

        AVDRMFrameDescriptor *drmdesc = reinterpret_cast<AVDRMFrameDescriptor*>(av_mallocz(sizeof(*drmdesc)));
        drmdesc->nb_objects = static_cast<int>(vadesc.num_objects);
        for (uint i = 0; i < vadesc.num_objects; i++)
        {
            drmdesc->objects[i].fd              = vadesc.objects[i].fd;
            drmdesc->objects[i].size            = vadesc.objects[i].size;
            drmdesc->objects[i].format_modifier = vadesc.objects[i].drm_format_modifier;
        }
        drmdesc->nb_layers = static_cast<int>(vadesc.num_layers);
        for (uint i = 0; i < vadesc.num_layers; i++)
        {
            drmdesc->layers[i].format    = vadesc.layers[i].drm_format;
            drmdesc->layers[i].nb_planes = static_cast<int>(vadesc.layers[i].num_planes);
            for (uint j = 0; j < vadesc.layers[i].num_planes; j++)
            {
                drmdesc->layers[i].planes[j].object_index = static_cast<int>(vadesc.layers[i].object_index[j]);
                drmdesc->layers[i].planes[j].offset       = vadesc.layers[i].offset[j];
                drmdesc->layers[i].planes[j].pitch        = vadesc.layers[i].pitch[j];
            }
        }
        m_drmFrames.insert(Id, drmdesc);
    }

    if (!m_drmFrames.contains(Id))
        return result;

    unsigned char* temp = Frame->buf;
    Frame->buf = reinterpret_cast<unsigned char*>(m_drmFrames[Id]);
    Frame->codec = FMT_DRMPRIME;
    Frame->pix_fmt = AV_PIX_FMT_DRM_PRIME;
    result = m_drmPrimeInterop->Acquire(Context, ColourSpace, Frame, Scan);
    Frame->buf = temp;
    Frame->codec = FMT_VAAPI;
    Frame->pix_fmt = AV_PIX_FMT_VAAPI;
    return result;
}

void MythVAAPIInteropDRM::CleanupDRMPRIME(void)
{
    if (m_drmPrimeInterop)
        m_drmPrimeInterop->DecrRef();

    if (!m_drmFrames.isEmpty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Releasing %1 DRM descriptors").arg(m_drmFrames.size()));
        QHash<unsigned long long, AVDRMFrameDescriptor*>::iterator it = m_drmFrames.begin();
        for ( ; it != m_drmFrames.end(); ++it)
        {
            for (int i = 0; i < (*it)->nb_objects; i++)
                close((*it)->objects[i].fd);
            av_freep(&(*it));
        }
    }
}
