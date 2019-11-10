// MythTV
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

MythDRMPRIMEInterop::MythDRMPRIMEInterop(MythRenderOpenGL *Context)
  : MythOpenGLInterop(Context, DRMPRIME)
{
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

/*! \brief Copy the frame described by AVDRMFrameDescriptor to an OpenGLTexture.
 *
 * This code currently uses the OpenGL ES2.0 'version' of EGL_EXT_image_dma_buf_import.
 * The incoming DRM planes are combined into a single RGB texture, with limited hints
 * for colourspace handling. Hence there are no picture controls and currently
 * no deinterlacing support (although for the Raspberry Pi VC4 codecs and Amlogic
 * S905 drivers the interlacing flags do not appear to be passed through anyway).
 *
 * For OpenGL ES3.0 capable devices, we should be able pass the planes into the
 * driver as separate textures - which will allow full colourspace control and deinterlacing
 * (as for VAAPI DRM interop).
*/
vector<MythVideoTexture*> MythDRMPRIMEInterop::Acquire(MythRenderOpenGL *Context,
                                                      VideoColourSpace *ColourSpace,
                                                      VideoFrame *Frame, FrameScanType)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    AVDRMFrameDescriptor* drmdesc = VerifyBuffer(Context, Frame);
    if (!drmdesc)
        return result;

    // Disable picture attributes on first pass
    if (ColourSpace && m_openglTextures.isEmpty())
        ColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);

    OpenGLLocker locker(m_context);

    // Validate descriptor
    if (drmdesc->nb_layers != 1)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Invalid DRM PRIME layer count (%1)")
            .arg(drmdesc->nb_layers));
        return result;
    }

    // Create texture
    if (m_openglTextures.isEmpty())
    {
        vector<QSize> sizes;
        sizes.push_back(m_openglTextureSize);
        vector<MythVideoTexture*> textures = MythVideoTexture::CreateTextures(m_context, FMT_DRMPRIME, FMT_RGBA32, sizes);
        for (uint i = 0; i < textures.size(); ++i)
            textures[i]->m_target = GL_TEXTURE_EXTERNAL_OES;
        MythVideoTexture::SetTextureFilters(m_context, textures, QOpenGLTexture::Linear);
        m_openglTextures.insert(DUMMY_INTEROP_ID, textures);
    }

    if (!m_openglTextures.contains(DUMMY_INTEROP_ID))
        return result;
    result = m_openglTextures[DUMMY_INTEROP_ID];

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

    std::vector<EGLint> attrs = {
        EGL_LINUX_DRM_FOURCC_EXT,      static_cast<EGLint>(drmdesc->layers[0].format),
        EGL_WIDTH,                     Frame->width,
        EGL_HEIGHT,                    Frame->height,
        EGL_YUV_COLOR_SPACE_HINT_EXT,  colourspace,
        EGL_SAMPLE_RANGE_HINT_EXT,     Frame->colorrange == AVCOL_RANGE_JPEG ? EGL_YUV_FULL_RANGE_EXT : EGL_YUV_NARROW_RANGE_EXT,
        EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT,   EGL_YUV_CHROMA_SITING_0_EXT,
        EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_EXT,
        EGL_DMA_BUF_PLANE0_FD_EXT,     drmdesc->objects[drmdesc->layers[0].planes[0].object_index].fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(drmdesc->layers[0].planes[0].offset),
        EGL_DMA_BUF_PLANE0_PITCH_EXT,  static_cast<EGLint>(drmdesc->layers[0].planes[0].pitch)
    };

    if (drmdesc->layers[0].nb_planes > 1)
    {
        attrs.push_back(EGL_DMA_BUF_PLANE1_FD_EXT);
        attrs.push_back(drmdesc->objects[drmdesc->layers[0].planes[1].object_index].fd);
        attrs.push_back(EGL_DMA_BUF_PLANE1_OFFSET_EXT);
        attrs.push_back(static_cast<EGLint>(drmdesc->layers[0].planes[1].offset));
        attrs.push_back(EGL_DMA_BUF_PLANE1_PITCH_EXT);
        attrs.push_back(static_cast<EGLint>(drmdesc->layers[0].planes[1].pitch));
    }

    if (drmdesc->layers[0].nb_planes > 2)
    {
        attrs.push_back(EGL_DMA_BUF_PLANE2_FD_EXT);
        attrs.push_back(drmdesc->objects[drmdesc->layers[0].planes[2].object_index].fd);
        attrs.push_back(EGL_DMA_BUF_PLANE2_OFFSET_EXT);
        attrs.push_back(static_cast<EGLint>(drmdesc->layers[0].planes[2].offset));
        attrs.push_back(EGL_DMA_BUF_PLANE2_PITCH_EXT);
        attrs.push_back(static_cast<EGLint>(drmdesc->layers[0].planes[2].pitch));
    }
    attrs.push_back(EGL_NONE);

    EGLImageKHR image = m_context->eglCreateImageKHR(m_context->GetEGLDisplay(), EGL_NO_CONTEXT,
                                                     EGL_LINUX_DMA_BUF_EXT, nullptr, &attrs[0]);
    if (!image)
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage '%1'").arg(m_context->GetEGLError()));

    MythVideoTexture *texture = result[0];
    m_context->glBindTexture(texture->m_target, texture->m_textureId);
    m_context->eglImageTargetTexture2DOES(texture->m_target, image);
    m_context->glBindTexture(texture->m_target, 0);
    m_context->eglDestroyImageKHR(m_context->GetEGLDisplay(), image);

    return result;
}
