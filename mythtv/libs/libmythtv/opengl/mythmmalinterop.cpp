// MythTV
#include "videocolourspace.h"
#include "fourcc.h"
#include "mythmmalinterop.h"

extern "C" {
#include "libavutil/pixdesc.h"
}

// EGL
#include "mythegldefs.h"

#define LOC QString("MMALInterop: ")

MythMMALInterop::MythMMALInterop(MythRenderOpenGL *Context)
  : MythOpenGLInterop(Context, MMAL)
{
}

MythMMALInterop::~MythMMALInterop()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Destructor");
}

MythOpenGLInterop::Type MythMMALInterop::GetInteropType(VideoFrameType Format)
{
    if (FMT_MMAL != Format)
        return Unsupported;

    MythRenderOpenGL* context = MythRenderOpenGL::GetOpenGLRender();
    if (!context)
        return Unsupported;

    OpenGLLocker locker(context);
    if (!context->IsEGL())
        return Unsupported;

    // MMAL interop only works with the closed source driver
    QString renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    if (!renderer.contains("VideoCore", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Interop requires the closed source/Broadcom driver - not '%1'")
            .arg(renderer));
        return Unsupported;
    }

    return context->hasExtension("GL_OES_EGL_image") ? MMAL : Unsupported;
}

MythMMALInterop* MythMMALInterop::Create(MythRenderOpenGL *Context, Type InteropType)
{
    if (!Context)
        return nullptr;
    if (InteropType == MMAL)
        return new MythMMALInterop(Context);
    return nullptr;
}

MMAL_BUFFER_HEADER_T* MythMMALInterop::VerifyBuffer(MythRenderOpenGL *Context, VideoFrame *Frame)
{
    MMAL_BUFFER_HEADER_T* result = nullptr;

    if ((Frame->pix_fmt != AV_PIX_FMT_MMAL) || (Frame->codec != FMT_MMAL) ||
        !Frame->buf || !Frame->priv[0])
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid MMAL buffer %1 %2 %3 %4")
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

    result = reinterpret_cast<MMAL_BUFFER_HEADER_T*>(Frame->buf);
    return result;
}

vector<MythVideoTexture*> MythMMALInterop::Acquire(MythRenderOpenGL *Context,
                                                   VideoColourSpace *ColourSpace,
                                                   VideoFrame *Frame,
                                                   FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    MMAL_BUFFER_HEADER_T* buffer = VerifyBuffer(Context, Frame);
    if (!buffer)
        return result;

    // Disallow kernel GLSL deint. There are not enough texture units with the
    // Broadcom driver and we don't retain references
    Frame->deinterlace_allowed = (Frame->deinterlace_allowed & ~DEINT_HIGH) | DEINT_MEDIUM;

    // Update frame colourspace and initialise on first frame
    if (ColourSpace)
    {
        if (m_openglTextures.isEmpty())
            ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Deinterlacing
    if (is_interlaced(Scan))
    {
        // only shader support for now
        MythDeintType shader = GetDoubleRateOption(Frame, DEINT_SHADER);
        if (shader)
        {
            Frame->deinterlace_double = Frame->deinterlace_double | DEINT_SHADER;
        }
        else
        {
            shader = GetSingleRateOption(Frame, DEINT_SHADER);
            if (shader)
                Frame->deinterlace_single = Frame->deinterlace_single | DEINT_SHADER;
        }
    }

    OpenGLLocker locker(m_context);

    VideoFrameType format = FMT_YV12;
    uint count = planes(format);

    if (m_openglTextures.isEmpty())
    {
        vector<QSize> sizes;
        for (uint plane = 0 ; plane < count; ++plane)
        {
            QSize size(width_for_plane(format, Frame->width, plane), height_for_plane(format, Frame->height, plane));
            sizes.push_back(size);
        }

        vector<MythVideoTexture*> textures =
                MythVideoTexture::CreateTextures(m_context, FMT_MMAL, format, sizes,GL_TEXTURE_EXTERNAL_OES);
        if (textures.size() != count)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create all textures");

        for (uint i = 0; i < textures.size(); ++i)
        {
            textures[i]->m_allowGLSLDeint = true;
            textures[i]->m_flip = false;
        }
        m_openglTextures.insert(DUMMY_INTEROP_ID, textures);
    }

    if (!m_openglTextures.contains(DUMMY_INTEROP_ID))
        return result;
    result = m_openglTextures[DUMMY_INTEROP_ID];

    for (uint plane = 0; plane < result.size(); ++plane)
    {
        MythVideoTexture* texture = result[plane];
        EGLenum target = EGL_IMAGE_BRCM_MULTIMEDIA_Y;
        if (plane == 1)
            target = EGL_IMAGE_BRCM_MULTIMEDIA_U;
        else if (plane == 2)
            target = EGL_IMAGE_BRCM_MULTIMEDIA_V;

        EGLImageKHR image = m_context->eglCreateImageKHR(m_context->GetEGLDisplay(), EGL_NO_CONTEXT, target,
                                                        (EGLClientBuffer)buffer->data, nullptr);
        if (!image)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage for plane %1 %2")
                .arg(plane).arg(m_context->GetEGLError()));

        m_context->glBindTexture(texture->m_target, texture->m_textureId);
        m_context->eglImageTargetTexture2DOES(texture->m_target, image);
        m_context->glBindTexture(texture->m_target, 0);
        m_context->eglDestroyImageKHR(m_context->GetEGLDisplay(), image);
    }

    return result;
}
