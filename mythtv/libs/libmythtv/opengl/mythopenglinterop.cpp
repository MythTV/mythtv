// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythui/opengl/mythrenderopengl.h"

#include "mythplayerui.h"
#include "mythvideocolourspace.h"
#include "opengl/mythopenglinterop.h"

#ifdef USING_VAAPI
#include "mythvaapiinterop.h"
#endif
#ifdef USING_VTB
#include "mythvtbinterop.h"
#endif
#ifdef USING_MEDIACODEC
#include "decoders/mythmediacodeccontext.h"
#endif
#ifdef USING_VDPAU
#include "mythvdpauinterop.h"
#endif
#ifdef USING_NVDEC
#include "mythnvdecinterop.h"
#endif
#ifdef USING_MMAL
#include "mythmmalinterop.h"
#endif
#ifdef USING_EGL
#include "mythdrmprimeinterop.h"
#endif

#define LOC QString("OpenGLInterop: ")

void MythOpenGLInterop::GetTypes(MythRender* Render, InteropMap& Types)
{
    auto * openglrender = dynamic_cast<MythRenderOpenGL*>(Render);
    if (!openglrender)
        return;

#ifdef USING_MEDIACODEC
    Types[FMT_MEDIACODEC] = { GL_MEDIACODEC };
#endif

#ifdef USING_VDPAU
    MythVDPAUInterop::GetVDPAUTypes(openglrender, Types);
#endif

#ifdef USING_VAAPI
    MythVAAPIInterop::GetVAAPITypes(openglrender, Types);
#endif

#ifdef USING_EGL
    MythDRMPRIMEInterop::GetDRMTypes(openglrender, Types);
#endif

#ifdef USING_MMAL
    MythMMALInterop::GetMMALTypes(openglrender, Types);
#endif

#ifdef USING_VTB
    MythVTBInterop::GetVTBTypes(openglrender, Types);
#endif

#ifdef USING_NVDEC
    MythNVDECInterop::GetNVDECTypes(openglrender, Types);
#endif
}

std::vector<MythVideoTextureOpenGL*>
MythOpenGLInterop::Retrieve(MythRenderOpenGL *Context,
                            MythVideoColourSpace *ColourSpace,
                            MythVideoFrame *Frame,
                            FrameScanType Scan)
{
    std::vector<MythVideoTextureOpenGL*> result;
    if (!(Context && Frame))
        return result;

    if (!Frame->m_priv[1] || !MythVideoFrame::HardwareFormat(Frame->m_type) ||
         (Frame->m_type != MythAVUtil::PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->m_pixFmt))))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not a valid hardware frame");
        return result;
    }

    MythOpenGLInterop* interop = nullptr;
    if ((Frame->m_type == FMT_VTB)  || (Frame->m_type == FMT_MEDIACODEC) ||
        (Frame->m_type == FMT_MMAL) || (Frame->m_type == FMT_DRMPRIME))
    {
        interop = reinterpret_cast<MythOpenGLInterop*>(Frame->m_priv[1]);
    }
    else
    {
        // Unpick
        auto* buffer = reinterpret_cast<AVBufferRef*>(Frame->m_priv[1]);
        if (!buffer || !buffer->data)
            return result;
        if (Frame->m_type == FMT_NVDEC)
        {
            auto* context = reinterpret_cast<AVHWDeviceContext*>(buffer->data);
            if (!context || !context->user_opaque)
                return result;
            interop = reinterpret_cast<MythOpenGLInterop*>(context->user_opaque);
        }
        else
        {
            auto* frames = reinterpret_cast<AVHWFramesContext*>(buffer->data);
            if (!frames || !frames->user_opaque)
                return result;
            interop = reinterpret_cast<MythOpenGLInterop*>(frames->user_opaque);
        }
    }

    if (interop)
        return interop->Acquire(Context, ColourSpace, Frame, Scan);
    LOG(VB_GENERAL, LOG_ERR, LOC + "No OpenGL interop found");
    return result;
}

MythOpenGLInterop::MythOpenGLInterop(MythRenderOpenGL *Context, InteropType Type, MythPlayerUI *Player)
  : MythInteropGPU(Context, Type, Player),
    m_openglContext(Context)
{
}

MythOpenGLInterop::~MythOpenGLInterop()
{
    MythOpenGLInterop::DeleteTextures();
}

std::vector<MythVideoTextureOpenGL*>
MythOpenGLInterop::Acquire(MythRenderOpenGL* /*Context*/,
                           MythVideoColourSpace* /*ColourSpace*/,
                           MythVideoFrame* /*Frame*/, FrameScanType /*Scan*/)
{
    return {};
}

void MythOpenGLInterop::DeleteTextures()
{
    if (m_openglContext && !m_openglTextures.isEmpty())
    {
        OpenGLLocker locker(m_openglContext);
        int count = 0;
        for (auto it = m_openglTextures.constBegin(); it != m_openglTextures.constEnd(); ++it)
        {
            std::vector<MythVideoTextureOpenGL*> textures = it.value();
            for (auto & texture : textures)
            {
                if (texture->m_textureId)
                    m_openglContext->glDeleteTextures(1, &texture->m_textureId);
                MythVideoTextureOpenGL::DeleteTexture(m_openglContext, texture);
                count++;
            }
            textures.clear();
        }
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Deleted %1 textures in %2 groups")
            .arg(count).arg(m_openglTextures.size()));
        m_openglTextures.clear();
    }
}
