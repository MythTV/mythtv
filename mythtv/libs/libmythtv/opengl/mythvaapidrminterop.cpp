// MythTV
#include "mythcorecontext.h"
#include "videocolourspace.h"
#include "fourcc.h"
#include "mythvaapidrminterop.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_drm.h"
}

// Std
#include <unistd.h>

#define LOC QString("VAAPIDRM: ")

MythVAAPIInteropDRM::MythVAAPIInteropDRM(MythRenderOpenGL *Context)
  : MythVAAPIInterop(Context, VAAPIEGLDRM),
    MythEGLDMABUF(Context)
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

    // DRM PRIME is preferred as it explicitly sets the fourcc's for each layer -
    // so we don't have to guess. But it is not available with older libva and
    // there are reports it does not work with some Radeon drivers
    m_usePrime = TestPrimeInterop();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using %1 for interop")
        .arg(m_usePrime ? "DRM PRIME" : "VAAPI handle"));
}

MythVAAPIInteropDRM::~MythVAAPIInteropDRM()
{
    OpenGLLocker locker(m_context);
    CleanupDRMPRIME();
    CleanupReferenceFrames();
    MythVAAPIInteropDRM::DestroyDeinterlacer();
    MythVAAPIInteropDRM::DeleteTextures();
    if (m_drmFile.isOpen())
        m_drmFile.close();
}

void MythVAAPIInteropDRM::DeleteTextures(void)
{
    OpenGLLocker locker(m_context);

    if (!m_openglTextures.isEmpty() && m_context->IsEGL())
    {
        int count = 0;
        for (auto it = m_openglTextures.constBegin() ; it != m_openglTextures.constEnd(); ++it)
        {
            vector<MythVideoTexture*> textures = it.value();
            for (auto & texture : textures)
            {
                if (texture->m_data)
                {
                    m_context->eglDestroyImageKHR(m_context->GetEGLDisplay(), texture->m_data);
                    texture->m_data = nullptr;
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
        MythVAAPIInteropDRM::DeleteTextures();
        CleanupDRMPRIME();
    }
    MythVAAPIInterop::DestroyDeinterlacer();
}

void MythVAAPIInteropDRM::PostInitDeinterlacer(void)
{
    // remove the old, non-deinterlaced frame cache
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting progressive frame cache");
    DeleteTextures();
    CleanupDRMPRIME();
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
    if (!m_referenceFrames.empty() &&
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

    auto next = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[0]->data));
    auto current = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[size > 1 ? 1 : 0]->data));
    auto last = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[size > 2 ? 2 : 0]->data));

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
    auto discontinuity = abs(Frame->frameCounter - m_discontinuityCounter) > 1;

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
        {
            if (discontinuity)
                DestroyDeinterlacer();
            id = Deinterlace(Frame, id, Scan);
        }

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
        if (discontinuity)
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
        return m_openglTextures[id];
    }

    OpenGLLocker locker(m_context);
    result = m_usePrime ? AcquirePrime(id, Context, Frame): AcquireVAAPI(id, Context, Frame);
    m_openglTextures.insert(id, result);
    if (needreferenceframes)
        return GetReferenceFrames();
    return result;
}

#ifndef DRM_FORMAT_R8
#define MKTAG2(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | (static_cast<unsigned>(d) << 24))
#define DRM_FORMAT_R8       MKTAG2('R', '8', ' ', ' ')
#define DRM_FORMAT_GR88     MKTAG2('G', 'R', '8', '8')
#define DRM_FORMAT_R16      MKTAG2('R', '1', '6', ' ')
#define DRM_FORMAT_GR32     MKTAG2('G', 'R', '3', '2')
#endif

vector<MythVideoTexture*> MythVAAPIInteropDRM::AcquireVAAPI(VASurfaceID Id,
                                                            MythRenderOpenGL *Context,
                                                            VideoFrame *Frame)
{
    vector<MythVideoTexture*> result;

    VAImage vaimage;
    memset(&vaimage, 0, sizeof(vaimage));
    vaimage.buf = vaimage.image_id = VA_INVALID_ID;
    INIT_ST;
    va_status = vaDeriveImage(m_vaDisplay, Id, &vaimage);
    CHECK_ST;
    uint numplanes = vaimage.num_planes;

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
        if (numplanes != planes(format))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Inconsistent plane count %1 != %2")
                .arg(numplanes).arg(planes(format)));
        }
        else
        {
            AVDRMFrameDescriptor drmdesc;
            memset(&drmdesc, 0, sizeof(drmdesc));
            drmdesc.nb_objects = 1;
            drmdesc.nb_layers = static_cast<int>(numplanes);
            drmdesc.objects[0].fd = static_cast<int>(vabufferinfo.handle);
            drmdesc.objects[0].size = 0;
            drmdesc.objects[0].format_modifier = 0;

            for (uint i = 0; i < numplanes; ++i)
            {
                uint32_t fourcc = (format == FMT_P010) ? DRM_FORMAT_R16 : DRM_FORMAT_R8;
                if (i > 0)
                    fourcc = (format == FMT_P010) ? DRM_FORMAT_GR32 : DRM_FORMAT_GR88;
                drmdesc.layers[i].nb_planes = 1;
                drmdesc.layers[i].format = fourcc;
                drmdesc.layers[i].planes[0].object_index = 0;
                drmdesc.layers[i].planes[0].pitch = vaimage.pitches[i];
                drmdesc.layers[i].planes[0].offset = vaimage.offsets[i];
            }

            result = CreateTextures(&drmdesc, Context, Frame, false);
        }
    }

    va_status = vaReleaseBufferHandle(m_vaDisplay, vaimage.buf);
    CHECK_ST;
    va_status = vaDestroyImage(m_vaDisplay, vaimage.image_id);
    CHECK_ST;

    return result;
}

VideoFrameType MythVAAPIInteropDRM::VATypeToMythType(uint32_t Fourcc)
{
    switch (Fourcc)
    {
        case VA_FOURCC_IYUV:
        case VA_FOURCC_I420: return FMT_YV12;
        case VA_FOURCC_NV12: return FMT_NV12;
        case VA_FOURCC_YUY2:
        case VA_FOURCC_UYVY: return FMT_YUY2;
#if defined (VA_FOURCC_P010)
        case VA_FOURCC_P010: return FMT_P010;
#endif
#if defined (VA_FOURCC_P016)
        case VA_FOURCC_P016: return FMT_P016;
#endif
        case VA_FOURCC_ARGB: return FMT_ARGB32;
        case VA_FOURCC_RGBA: return FMT_RGBA32;
    }
    return FMT_NONE;
}

bool MythVAAPIInteropDRM::IsSupported(MythRenderOpenGL *Context)
{
    return HaveDMABuf(Context);
}

#if VA_CHECK_VERSION(1, 1, 0)
static inline void VADRMtoPRIME(VADRMPRIMESurfaceDescriptor* VaDRM, AVDRMFrameDescriptor* Prime)
{
    Prime->nb_objects = static_cast<int>(VaDRM->num_objects);
    for (uint i = 0; i < VaDRM->num_objects; i++)
    {
        Prime->objects[i].fd              = VaDRM->objects[i].fd;
        Prime->objects[i].size            = VaDRM->objects[i].size;
        Prime->objects[i].format_modifier = VaDRM->objects[i].drm_format_modifier;
    }
    Prime->nb_layers = static_cast<int>(VaDRM->num_layers);
    for (uint i = 0; i < VaDRM->num_layers; i++)
    {
        Prime->layers[i].format    = VaDRM->layers[i].drm_format;
        Prime->layers[i].nb_planes = static_cast<int>(VaDRM->layers[i].num_planes);
        for (uint j = 0; j < VaDRM->layers[i].num_planes; j++)
        {
            Prime->layers[i].planes[j].object_index = static_cast<int>(VaDRM->layers[i].object_index[j]);
            Prime->layers[i].planes[j].offset       = VaDRM->layers[i].offset[j];
            Prime->layers[i].planes[j].pitch        = VaDRM->layers[i].pitch[j];
        }
    }
}
#endif

/*! \brief Export the given VideoFrame as a DRM PRIME descriptor
 *
 * This is funcionally equivalent to the 'regular' VAAPI version but is useful
 * for testing DRM PRIME functionality on desktops.
*/
vector<MythVideoTexture*> MythVAAPIInteropDRM::AcquirePrime(VASurfaceID Id,
                                                            MythRenderOpenGL *Context,
                                                            VideoFrame *Frame)
{
    vector<MythVideoTexture*> result;

#if VA_CHECK_VERSION(1, 1, 0)
    if (!m_drmFrames.contains(Id))
    {
        INIT_ST;
        uint32_t exportflags = VA_EXPORT_SURFACE_SEPARATE_LAYERS | VA_EXPORT_SURFACE_READ_ONLY;
        VADRMPRIMESurfaceDescriptor vadesc;
        va_status = vaExportSurfaceHandle(m_vaDisplay, Id,
                                          VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                          exportflags, &vadesc);
        CHECK_ST;

        auto *drmdesc = reinterpret_cast<AVDRMFrameDescriptor*>(av_mallocz(sizeof(AVDRMFrameDescriptor)));
        VADRMtoPRIME(&vadesc, drmdesc);
        m_drmFrames.insert(Id, drmdesc);
    }

    if (!m_drmFrames.contains(Id))
        return result;
    result = CreateTextures(m_drmFrames[Id], Context, Frame, false);
#else
    (void)Id;
    (void)Context;
    (void)Frame;
#endif
    return result;
}

void MythVAAPIInteropDRM::CleanupDRMPRIME(void)
{
    if (m_drmFrames.isEmpty())
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Releasing %1 DRM descriptors").arg(m_drmFrames.size()));
    for (auto it = m_drmFrames.begin() ; it != m_drmFrames.end(); ++it)
    {
        for (int i = 0; i < (*it)->nb_objects; i++)
            close((*it)->objects[i].fd);
        av_freep(&(*it));
    }
    m_drmFrames.clear();
}

bool MythVAAPIInteropDRM::TestPrimeInterop(void)
{
    static bool s_supported = false;
#if VA_CHECK_VERSION(1, 1, 0)
    static bool s_checked = false;

    if (s_checked)
        return s_supported;
    s_checked = true;

    OpenGLLocker locker(m_context);

    VASurfaceID surface = 0;

    VASurfaceAttrib attribs = {};
    attribs.flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs.type = VASurfaceAttribPixelFormat;
    attribs.value.type = VAGenericValueTypeInteger;
    attribs.value.value.i = VA_FOURCC_NV12;

    if (vaCreateSurfaces(m_vaDisplay, VA_RT_FORMAT_YUV420, 1920, 1080,
                         &surface, 1, &attribs, 1) == VA_STATUS_SUCCESS)
    {
        VADRMPRIMESurfaceDescriptor vadesc;
        VAStatus status = vaExportSurfaceHandle(m_vaDisplay, surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                       VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                       &vadesc);
        if (status == VA_STATUS_SUCCESS)
        {
            VideoFrame frame {};
            init(&frame, FMT_DRMPRIME, nullptr, 1920, 1080, 0);
            frame.sw_pix_fmt = AV_PIX_FMT_NV12;
            AVDRMFrameDescriptor drmdesc;
            memset(&drmdesc, 0, sizeof(drmdesc));
            VADRMtoPRIME(&vadesc, &drmdesc);
            vector<MythVideoTexture*> textures = CreateTextures(&drmdesc, m_context, &frame, false);

            if (!textures.empty())
            {
                s_supported = true;
                for (auto & texture : textures)
                    s_supported &= texture->m_data && texture->m_textureId;
                ClearDMATextures(m_context, textures);
            }
            for (uint32_t i = 0; i < vadesc.num_objects; ++i)
                close(vadesc.objects[i].fd);
        }
        vaDestroySurfaces(m_vaDisplay, &surface, 1);
    }
#endif
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VAAPI DRM PRIME interop is %1supported")
        .arg(s_supported ? "" : "not "));
    return s_supported;
}
