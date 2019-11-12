// MythTV
#include "fourcc.h"
#include "videocolourspace.h"
#include "mythdrmprimeinterop.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_drm.h"
#include "libavutil/pixdesc.h"
}

// EGL
#include "mythegldefs.h"

#define LOC QString("DRMInterop: ")

// Note: EnableCacheing is enabled by default. When disabled, this instance is owned
// by another interop class which will manage cacheing itself and will also expect
// that other features such as display attributes and deinterlacer settings are not
// overridden.
MythDRMPRIMEInterop::MythDRMPRIMEInterop(MythRenderOpenGL *Context, bool EnableCacheing)
  : MythOpenGLInterop(Context, DRMPRIME),
    m_enableCacheing(EnableCacheing)
{
    OpenGLLocker locker(m_context);
    m_openGLES = m_context->isOpenGLES();
    QSurfaceFormat fmt = m_context->format();
    if (fmt.majorVersion() >= 3)
    {
        m_fullYUVSupport = true;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Full YUV DRM support available");
    }
    m_hasModifiers = m_context->HasEGLExtension("EGL_EXT_image_dma_buf_import_modifiers");
}

MythDRMPRIMEInterop::~MythDRMPRIMEInterop()
{
    DeleteTextures();
}

void MythDRMPRIMEInterop::DeleteTextures(void)
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

    MythOpenGLInterop::DeleteTextures();
}

MythDRMPRIMEInterop* MythDRMPRIMEInterop::Create(MythRenderOpenGL *Context, Type InteropType)
{
    if (Context && (InteropType == DRMPRIME))
        return new MythDRMPRIMEInterop(Context);
    return nullptr;
}

MythOpenGLInterop::Type MythDRMPRIMEInterop::GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context)
{
    // TODO - this should be tied to pix_fmt (i.e. AV_PIX_FMT_DRM_PRIME) not codec.
    // Probably applies to all interops
    if (!codec_is_v4l2(CodecId))
        return Unsupported;

    if (!Context)
        Context = MythRenderOpenGL::GetOpenGLRender();
    if (!Context)
        return Unsupported;

    OpenGLLocker locker(Context);
    return (Context->IsEGL() && Context->hasExtension("GL_OES_EGL_image") &&
            Context->HasEGLExtension("EGL_EXT_image_dma_buf_import")) ? DRMPRIME : Unsupported;
}

AVDRMFrameDescriptor* MythDRMPRIMEInterop::VerifyBuffer(MythRenderOpenGL *Context, VideoFrame *Frame)
{
    AVDRMFrameDescriptor* result = nullptr;

    if ((Frame->pix_fmt != AV_PIX_FMT_DRM_PRIME) || (Frame->codec != FMT_DRMPRIME) ||
        !Frame->buf || !Frame->priv[0])
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid DRM PRIME buffer %1 %2 %3 %4")
            .arg(Frame->buf != nullptr).arg(Frame->priv[0] != nullptr)
            .arg(format_description(Frame->codec))
            .arg(av_get_pix_fmt_name(static_cast<AVPixelFormat>(Frame->pix_fmt))));
        return result;
    }

    // Sanity check the context
    if (m_context != Context)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Mismatched OpenGL contexts!");
        return result;
    }

    // Check size
    QSize surfacesize(Frame->width, Frame->height);
    if (m_openglTextureSize != surfacesize)
    {
        if (!m_openglTextureSize.isEmpty())
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Video texture size changed!");
        m_openglTextureSize = surfacesize;
    }

    return  reinterpret_cast<AVDRMFrameDescriptor*>(Frame->buf);
}

static void inline DebugDRMFrame(AVDRMFrameDescriptor* Desc)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("DRM frame: Layers %1 Objects %2")
        .arg(Desc->nb_layers).arg(Desc->nb_objects));
    for (int i = 0; i < Desc->nb_layers; ++i)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Layer %1: Format %2 Planes %3")
            .arg(i).arg(fourcc_str(static_cast<int>(Desc->layers[i].format)))
            .arg(Desc->layers[i].nb_planes));
        for (int j = 0; j < Desc->layers[i].nb_planes; ++j)
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("  Plane %1: Index %2 Offset %3 Pitch %4")
                .arg(j).arg(Desc->layers[i].planes[j].object_index)
                .arg(Desc->layers[i].planes[j].offset).arg(Desc->layers[i].planes[j].pitch));
    }
    for (int i = 0; i < Desc->nb_objects; ++i)
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Object: %1 FD %2 Mods 0x%3")
            .arg(i).arg(Desc->objects[i].fd).arg(Desc->objects[i].format_modifier, 0 , 16));
}

/*! \brief Copy the frame described by AVDRMFrameDescriptor to an OpenGLTexture.
 *
 * This code uses the OpenGL ES2.0 'version' of EGL_EXT_image_dma_buf_import.
 * The incoming DRM planes are combined into a single RGB texture, with limited hints
 * for colourspace handling. Hence there are no picture controls and currently
 * no deinterlacing support (although for the Raspberry Pi VC4 codecs and Amlogic
 * S905 drivers the interlacing flags do not appear to be passed through anyway).
 *
 * For OpenGL ES3.0 capable devices, we call AcquireYUV which should return a
 * complete set of textures representing the YUV frame.
*/
vector<MythVideoTexture*> MythDRMPRIMEInterop::Acquire(MythRenderOpenGL *Context,
                                                      VideoColourSpace *ColourSpace,
                                                      VideoFrame *Frame, FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    AVDRMFrameDescriptor* drmdesc = VerifyBuffer(Context, Frame);
    if (!drmdesc)
        return result;

    OpenGLLocker locker(m_context);

    // Full YUV support (GL >= 3)
    if (m_fullYUVSupport)
        return AcquireYUV(drmdesc, Context, ColourSpace, Frame, Scan);

    // Validate descriptor
    if (drmdesc->nb_layers != 1)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Invalid DRM PRIME layer count (%1)")
            .arg(drmdesc->nb_layers));
        return result;
    }

    if (m_enableCacheing)
    {
        // Disable picture attributes on first pass
        if (ColourSpace && m_openglTextures.isEmpty())
            ColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);

        // Disable shader based deinterlacing for RGBA frames
        Frame->deinterlace_allowed = Frame->deinterlace_allowed & ~DEINT_SHADER;
    }

    // do we need a new texture
    unsigned long long id = reinterpret_cast<unsigned long long>(drmdesc);
    if (!m_openglTextures.contains(id))
    {
        if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
            DebugDRMFrame(drmdesc);

        vector<QSize> sizes;
        sizes.push_back(m_openglTextureSize);
        vector<MythVideoTexture*> textures = MythVideoTexture::CreateTextures(m_context, FMT_DRMPRIME, FMT_RGBA32, sizes);
        if (m_openGLES)
            for (uint i = 0; i < textures.size(); ++i)
                textures[i]->m_target = GL_TEXTURE_EXTERNAL_OES;
        MythVideoTexture::SetTextureFilters(m_context, textures, QOpenGLTexture::Linear);

        EGLint colourspace = EGL_ITU_REC709_EXT;
        switch (Frame->colorspace)
        {
            case AVCOL_SPC_BT470BG:
            case AVCOL_SPC_SMPTE170M:
            case AVCOL_SPC_SMPTE240M:
                colourspace = EGL_ITU_REC601_EXT;
                break;
            case AVCOL_SPC_BT2020_CL:
            case AVCOL_SPC_BT2020_NCL:
                colourspace = EGL_ITU_REC2020_EXT;
                break;
            default:
                if (Frame->width < 1280)
                    colourspace = EGL_ITU_REC601_EXT;
                break;
        }

        static const EGLint PLANE_FD[4] =
            { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
              EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT };
        static const EGLint PLANE_OFFSET[4] =
            { EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
              EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT };
        static const EGLint PLANE_PITCH[4] =
            { EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
              EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT };
        static const EGLint PLANE_MODLO[4] =
            { EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
              EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT };
        static const EGLint PLANE_MODHI[4] =
            { EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
              EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT };

        AVDRMLayerDescriptor* layer = &drmdesc->layers[0];

        QVector<EGLint> attribs = {
            EGL_LINUX_DRM_FOURCC_EXT,      static_cast<EGLint>(layer->format),
            EGL_WIDTH,                     Frame->width,
            EGL_HEIGHT,                    Frame->height,
            EGL_YUV_COLOR_SPACE_HINT_EXT,  colourspace,
            EGL_SAMPLE_RANGE_HINT_EXT,     Frame->colorrange == AVCOL_RANGE_JPEG ? EGL_YUV_FULL_RANGE_EXT : EGL_YUV_NARROW_RANGE_EXT,
            EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT,   EGL_YUV_CHROMA_SITING_0_EXT,
            EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_EXT
        };

        for (int plane = 0; plane < layer->nb_planes; ++plane)
        {
            AVDRMPlaneDescriptor* drmplane = &layer->planes[plane];
            attribs << PLANE_FD[plane]     << drmdesc->objects[drmplane->object_index].fd
                    << PLANE_OFFSET[plane] << static_cast<EGLint>(drmplane->offset)
                    << PLANE_PITCH[plane]  << static_cast<EGLint>(drmplane->pitch);
            if (m_hasModifiers && (drmdesc->objects[drmplane->object_index].format_modifier != 0 /* DRM_FORMAT_MOD_NONE*/))
            {
                attribs << PLANE_MODLO[plane]
                        << static_cast<EGLint>(drmdesc->objects[drmplane->object_index].format_modifier & 0xffffffff)
                        << PLANE_MODHI[plane]
                        << static_cast<EGLint>(drmdesc->objects[drmplane->object_index].format_modifier >> 32);
            }
        }
        attribs << EGL_NONE;

        EGLImageKHR image = m_context->eglCreateImageKHR(m_context->GetEGLDisplay(), EGL_NO_CONTEXT,
                                                         EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
        if (!image)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage '%1'").arg(m_context->GetEGLError()));
        MythVideoTexture *texture = textures[0];
        m_context->glBindTexture(texture->m_target, texture->m_textureId);
        m_context->eglImageTargetTexture2DOES(texture->m_target, image);
        m_context->glBindTexture(texture->m_target, 0);
        texture->m_data = static_cast<unsigned char *>(image);
        if (!m_enableCacheing)
            return textures;
        m_openglTextures.insert(id, textures);
    }

    if (!m_openglTextures.contains(id))
        return result;
    return m_openglTextures[id];
}

vector<MythVideoTexture*> MythDRMPRIMEInterop::AcquireYUV(AVDRMFrameDescriptor* Desc,
                                                          MythRenderOpenGL *,
                                                          VideoColourSpace *ColourSpace,
                                                          VideoFrame *Frame, FrameScanType)
{
    vector<MythVideoTexture*> result;


    // Check frame format
    VideoFrameType format = PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->sw_pix_fmt));

    OpenGLLocker locker(m_context);
    uint count = static_cast<uint>(Desc->nb_layers);

    // Validate planes
    if (count != planes(format))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Inconsistent plane count (%1 != %2)")
            .arg(count).arg(planes(format)));
        return result;
    }

    if (m_enableCacheing)
    {
        // Update frame colourspace and initialise on first frame
        if (ColourSpace)
        {
            if (m_openglTextures.isEmpty())
                ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
            ColourSpace->UpdateColourSpace(Frame);
        }

        // Enable shader based deinterlacing for YUV frames IF we are the owner
        Frame->deinterlace_allowed = Frame->deinterlace_allowed | DEINT_SHADER;
    }

    // return cached texture if available
    unsigned long long id = reinterpret_cast<unsigned long long>(Desc);
    if (m_openglTextures.contains(id))
        return m_openglTextures[id];

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
        DebugDRMFrame(Desc);

    // Create textures
    // N.B. this works for YV12/NV12 but will probably break for YUV422 etc
    vector<QSize> sizes;
    for (uint plane = 0 ; plane < count; ++plane)
    {
        int width = Frame->width >> ((plane > 0) ? 1 : 0);
        int height = Frame->height >> ((plane > 0) ? 1 : 0);
        sizes.push_back(QSize(width, height));
    }
    vector<MythVideoTexture*> textures = MythVideoTexture::CreateTextures(m_context, FMT_DRMPRIME, format, sizes);
    for (uint i = 0; i < textures.size(); ++i)
        textures[i]->m_allowGLSLDeint = true;

    MythVideoTexture::SetTextureFilters(m_context, textures, QOpenGLTexture::Linear);
    result = textures;

    for (uint plane = 0; plane < result.size(); ++plane)
    {
        AVDRMLayerDescriptor* layer    = &Desc->layers[plane];
        AVDRMPlaneDescriptor* drmplane = &layer->planes[0];
        QVector<EGLint> attribs = {
            EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(layer->format),
            EGL_WIDTH,  result[plane]->m_size.width(),
            EGL_HEIGHT, result[plane]->m_size.height(),
            EGL_DMA_BUF_PLANE0_FD_EXT,     Desc->objects[drmplane->object_index].fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(drmplane->offset),
            EGL_DMA_BUF_PLANE0_PITCH_EXT,  static_cast<EGLint>(drmplane->pitch)
        };

        if (m_hasModifiers && (Desc->objects[drmplane->object_index].format_modifier != 0 /* DRM_FORMAT_MOD_NONE*/))
        {
            attribs << EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT
                    << static_cast<EGLint>(Desc->objects[drmplane->object_index].format_modifier & 0xffffffff)
                    << EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT
                    << static_cast<EGLint>(Desc->objects[drmplane->object_index].format_modifier >> 32);
        }

        attribs << EGL_NONE;

        EGLImageKHR image = m_context->eglCreateImageKHR(m_context->GetEGLDisplay(), EGL_NO_CONTEXT,
                                                         EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
        if (!image)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage for plane %1 %2")
                .arg(plane).arg(m_context->GetEGLError()));

        m_context->glBindTexture(result[plane]->m_target, result[plane]->m_textureId);
        m_context->eglImageTargetTexture2DOES(result[plane]->m_target, image);
        m_context->glBindTexture(result[plane]->m_target, 0);
        result[plane]->m_data = static_cast<unsigned char *>(image);
    }

    if (m_enableCacheing)
        m_openglTextures.insert(id, result);
    return result;
}

