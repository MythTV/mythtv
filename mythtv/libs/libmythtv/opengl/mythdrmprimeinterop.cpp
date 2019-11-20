// MythTV
#include "videocolourspace.h"
#include "mythdrmprimeinterop.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_drm.h"
#include "libavutil/pixdesc.h"
}

#define LOC QString("DRMInterop: ")

MythDRMPRIMEInterop::MythDRMPRIMEInterop(MythRenderOpenGL *Context)
  : MythOpenGLInterop(Context, DRMPRIME),
    MythEGLDMABUF(Context)
{
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
    if (!(codec_is_v4l2(CodecId) || codec_is_drmprime(CodecId)))
        return Unsupported;
    return HaveDMABuf(Context) ? DRMPRIME : Unsupported;
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

    bool firstpass = m_openglTextures.isEmpty();

    // return cached texture(s) if available
    unsigned long long id = reinterpret_cast<unsigned long long>(drmdesc);
    if (!m_openglTextures.contains(id))
    {
        result = CreateTextures(drmdesc, m_context, Frame);
        m_openglTextures.insert(id, result);
    }
    else
    {
        result = m_openglTextures[id];
    }

    if (result.size() > 0 ? format_is_yuv(result[0]->m_frameType) : false)
    {
        // YUV frame - enable picture attributes
        if (firstpass)
            ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
        ColourSpace->UpdateColourSpace(Frame);

        // Enable shader based deinterlacing for YUV frames
        Frame->deinterlace_allowed = Frame->deinterlace_allowed | DEINT_SHADER;
    }
    else
    {
        // RGB frame - disable picture attributes
        if (firstpass)
            ColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);

        // Disable shader based deinterlacing for RGBA frames
        Frame->deinterlace_allowed = Frame->deinterlace_allowed & ~DEINT_SHADER;
    }

    return result;
}

