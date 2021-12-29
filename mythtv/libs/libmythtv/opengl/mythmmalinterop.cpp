// MythTV
#include "mythvideocolourspace.h"
#include "fourcc.h"
#include "opengl/mythmmalinterop.h"

extern "C" {
#include "libavutil/pixdesc.h"
}

// EGL
#include "mythegldefs.h"

#define LOC QString("MMALInterop: ")

MythMMALInterop::MythMMALInterop(MythRenderOpenGL *Context)
  : MythOpenGLInterop(Context, GL_MMAL)
{
}

MythMMALInterop::~MythMMALInterop()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Destructor");
}

void MythMMALInterop::GetMMALTypes(MythRenderOpenGL* Render, MythInteropGPU::InteropMap& Types)
{
    OpenGLLocker locker(Render);
    if (!Render->IsEGL())
        return;

    // MMAL interop only works with the closed source driver
    QString renderer = reinterpret_cast<const char*>(Render->glGetString(GL_RENDERER));
    if (!renderer.contains("VideoCore", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Interop requires the closed source/Broadcom driver - not '%1'").arg(renderer));
        return;
    }

    if (Render->hasExtension("GL_OES_EGL_image"))
        Types[FMT_MMAL] = { GL_MMAL };
}

/*! \brief Create an MMAL interop
 *
 * \note This is called directly from the decoder - hence we do not attempt
 * to retrieve the list of supported types again. Assume it has already been verified.
*/
MythMMALInterop* MythMMALInterop::CreateMMAL(MythRenderOpenGL* Context)
{
    return Context ? new MythMMALInterop(Context) : nullptr;
}

MMAL_BUFFER_HEADER_T* MythMMALInterop::VerifyBuffer(MythRenderOpenGL *Context, MythVideoFrame *Frame)
{
    MMAL_BUFFER_HEADER_T* result = nullptr;

    if ((Frame->m_pixFmt != AV_PIX_FMT_MMAL) || (Frame->m_type != FMT_MMAL) ||
        !Frame->m_buffer || !Frame->m_priv[0])
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid MMAL buffer %1 %2 %3 %4")
            .arg(Frame->m_buffer != nullptr).arg(Frame->m_priv[0] != nullptr)
            .arg(MythVideoFrame::FormatDescription(Frame->m_type))
            .arg(av_get_pix_fmt_name(static_cast<AVPixelFormat>(Frame->m_pixFmt))));
        return result;
    }

    // Sanity check the context
    if (m_openglContext != Context)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Mismatched OpenGL contexts!");
        return result;
    }

    // Check size
    QSize surfacesize(Frame->m_width, Frame->m_height);
    if (m_textureSize != surfacesize)
    {
        if (!m_textureSize.isEmpty())
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Video texture size changed!");
        m_textureSize = surfacesize;
    }

    result = reinterpret_cast<MMAL_BUFFER_HEADER_T*>(Frame->m_buffer);
    return result;
}

std::vector<MythVideoTextureOpenGL*>
MythMMALInterop::Acquire(MythRenderOpenGL *Context,
                         MythVideoColourSpace *ColourSpace,
                         MythVideoFrame *Frame,
                         FrameScanType Scan)
{
    std::vector<MythVideoTextureOpenGL*> result;
    if (!Frame)
        return result;

    MMAL_BUFFER_HEADER_T* buffer = VerifyBuffer(Context, Frame);
    if (!buffer)
        return result;

    // Disallow kernel GLSL deint. There are not enough texture units with the
    // Broadcom driver and we don't retain references
    Frame->m_deinterlaceAllowed = (Frame->m_deinterlaceAllowed & ~DEINT_HIGH) | DEINT_MEDIUM;

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
        MythDeintType shader = Frame->GetDoubleRateOption(DEINT_SHADER);
        if (shader)
        {
            Frame->m_deinterlaceDouble = Frame->m_deinterlaceDouble | DEINT_SHADER;
        }
        else
        {
            shader = Frame->GetSingleRateOption(DEINT_SHADER);
            if (shader)
                Frame->m_deinterlaceSingle = Frame->m_deinterlaceSingle | DEINT_SHADER;
        }
    }

    OpenGLLocker locker(m_openglContext);

    VideoFrameType format = FMT_YV12;
    uint count = MythVideoFrame::GetNumPlanes(format);

    if (m_openglTextures.isEmpty())
    {
        std::vector<QSize> sizes;
        for (uint plane = 0 ; plane < count; ++plane)
        {
            QSize size(MythVideoFrame::GetWidthForPlane(format, Frame->m_width, plane),
                       MythVideoFrame::GetHeightForPlane(format, Frame->m_height, plane));
            sizes.push_back(size);
        }

        std::vector<MythVideoTextureOpenGL*> textures =
            MythVideoTextureOpenGL::CreateTextures(m_openglContext, FMT_MMAL, format, sizes,GL_TEXTURE_EXTERNAL_OES);
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
        MythVideoTextureOpenGL* texture = result[plane];
        EGLenum target = EGL_IMAGE_BRCM_MULTIMEDIA_Y;
        if (plane == 1)
            target = EGL_IMAGE_BRCM_MULTIMEDIA_U;
        else if (plane == 2)
            target = EGL_IMAGE_BRCM_MULTIMEDIA_V;

        EGLImageKHR image = m_openglContext->eglCreateImageKHR(m_openglContext->GetEGLDisplay(), EGL_NO_CONTEXT, target,
                                                        (EGLClientBuffer)buffer->data, nullptr);
        if (!image)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage for plane %1 %2")
                .arg(plane).arg(m_openglContext->GetEGLError()));

        m_openglContext->glBindTexture(texture->m_target, texture->m_textureId);
        m_openglContext->eglImageTargetTexture2DOES(texture->m_target, image);
        m_openglContext->glBindTexture(texture->m_target, 0);
        m_openglContext->eglDestroyImageKHR(m_openglContext->GetEGLDisplay(), image);
    }

    return result;
}
