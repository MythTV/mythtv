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
    MythDRMPRIMEInterop::DeleteTextures();
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
            auto it2 = textures.begin();
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

MythOpenGLInterop::Type MythDRMPRIMEInterop::GetInteropType(VideoFrameType Format)
{
    if (FMT_DRMPRIME != Format)
        return Unsupported;
    MythRenderOpenGL* context = MythRenderOpenGL::GetOpenGLRender();
    if (!context)
        return Unsupported;
    return HaveDMABuf(context) ? DRMPRIME : Unsupported;
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
                                                       VideoFrame *Frame,
                                                       FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    auto drmdesc = VerifyBuffer(Context, Frame);
    if (!drmdesc)
        return result;

    bool firstpass  = m_openglTextures.isEmpty();
    bool interlaced = is_interlaced(Scan);
    bool composed   = static_cast<uint>(drmdesc->nb_layers) == 1;
    auto id         = reinterpret_cast<unsigned long long>(drmdesc);

    // Separate texture for each plane
    if (!composed)
    {
        if (!m_openglTextures.contains(id))
        {
            result = CreateTextures(drmdesc, m_context, Frame);
            m_openglTextures.insert(id, result);
        }
        else
        {
            result = m_openglTextures[id];
        }

        if (!result.empty() ? format_is_yuv(result[0]->m_frameType) : false)
        {
            // YUV frame - enable picture attributes
            if (firstpass)
                ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
            ColourSpace->UpdateColourSpace(Frame);

            // Enable shader based deinterlacing for YUV frames
            Frame->deinterlace_allowed = Frame->deinterlace_allowed | DEINT_SHADER;
        }
        return result;
    }

    // Single RGB texture
    // Disable picture attributes
    if (firstpass)
        ColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);

    // Is deinterlacing selected? Accept any value as RGB frames can only be deinterlaced here
    bool doublerate = false;
    MythDeintType option = GetDoubleRateOption(Frame, DEINT_CPU | DEINT_SHADER | DEINT_DRIVER);
    if (option != DEINT_NONE)
        doublerate = true;
    else
        option = GetSingleRateOption(Frame, DEINT_CPU | DEINT_SHADER | DEINT_DRIVER);
    interlaced &= option != DEINT_NONE;

    // Clear redundant frame caches
    if (interlaced && !m_deinterlacing)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing progessive frame cache");
        DeleteTextures();
    }
    else if (m_deinterlacing && !interlaced)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing interlaced field cache");
        DeleteTextures();
    }
    m_deinterlacing = interlaced;

    if (!m_openglTextures.contains(id))
    {
        // This will create 2 half height textures representing the top and bottom fields
        // if deinterlacing
        result = CreateTextures(drmdesc, m_context, Frame,
                                m_deinterlacing ? kScan_Interlaced : kScan_Progressive);
        m_openglTextures.insert(id, result);
    }
    else
    {
        result = m_openglTextures[id];
    }

    if (m_deinterlacing)
    {
        result.clear();
        Frame->deinterlace_inuse = DEINT_DRIVER | DEINT_BASIC;
        Frame->deinterlace_inuse2x = doublerate;
        bool tff = Frame->interlaced_reversed ? !Frame->top_field_first : Frame->top_field_first;
        result.emplace_back(m_openglTextures[id].at(Scan == kScan_Interlaced ? (tff ? 1 : 0) : tff ? 0 : 1));
    }

    return result;
}

