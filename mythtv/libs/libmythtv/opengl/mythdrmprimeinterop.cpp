// MythTV
#include "mythplayerui.h"
#include "mythvideocolourspace.h"
#include "mythdrmprimeinterop.h"

#ifdef USING_DRM_VIDEO
#include "libmythui/mythmainwindow.h"
#include "libmythui/platforms/mythdisplaydrm.h"
#endif

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_drm.h"
#include "libavutil/pixdesc.h"
}

#define LOC QString("DRMInterop: ")

MythDRMPRIMEInterop::MythDRMPRIMEInterop(MythRenderOpenGL* Context, MythPlayerUI* Player, InteropType Type)
  : MythOpenGLInterop(Context, Type, Player),
    MythEGLDMABUF(Context)
{
}

MythDRMPRIMEInterop::~MythDRMPRIMEInterop()
{
#ifdef USING_DRM_VIDEO
    delete m_drm;
#endif
    MythDRMPRIMEInterop::DeleteTextures();
}

void MythDRMPRIMEInterop::DeleteTextures(void)
{
    OpenGLLocker locker(m_openglContext);

    if (!m_openglTextures.isEmpty() && m_openglContext->IsEGL())
    {
        int count = 0;
        for (auto it = m_openglTextures.constBegin(); it != m_openglTextures.constEnd(); ++it)
        {
            std::vector<MythVideoTextureOpenGL*> textures = it.value();
            for (auto & texture : textures)
            {
                if (texture->m_data)
                {
                    m_openglContext->eglDestroyImageKHR(m_openglContext->GetEGLDisplay(), texture->m_data);
                    texture->m_data = nullptr;
                    count++;
                }
            }
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Deleted %1 EGL images in %2 groups")
            .arg(count).arg(m_openglTextures.size()));
    }

    MythOpenGLInterop::DeleteTextures();
}

/*! \brief Create a DRM PRIME interop instance.
*/
MythDRMPRIMEInterop* MythDRMPRIMEInterop::CreateDRM(MythRenderOpenGL* Context, MythPlayerUI* Player)
{
    if (!(Player && Context))
        return nullptr;

    const auto & types = Player->GetInteropTypes();
    if (const auto & drm = types.find(FMT_DRMPRIME); drm != types.cend())
    {
        auto matchType = [](auto type){ return (type == GL_DRMPRIME) || (type == DRM_DRMPRIME); };
        auto it = std::find_if(drm->second.cbegin(), drm->second.cend(), matchType);
        if (it != drm->second.cend())
            return new MythDRMPRIMEInterop(Context, Player, *it);
    }
    return nullptr;
}

void MythDRMPRIMEInterop::GetDRMTypes(MythRenderOpenGL* Render, MythInteropGPU::InteropMap& Types)
{
    MythInteropGPU::InteropTypes drmtypes;

#ifdef USING_DRM_VIDEO
    if (MythDisplayDRM::DirectRenderingAvailable())
        drmtypes.emplace_back(DRM_DRMPRIME);
#endif

    if (HaveDMABuf(Render))
        drmtypes.emplace_back(GL_DRMPRIME);

    if (!drmtypes.empty())
        Types[FMT_DRMPRIME] = drmtypes;
}

AVDRMFrameDescriptor* MythDRMPRIMEInterop::VerifyBuffer(MythRenderOpenGL *Context, MythVideoFrame *Frame)
{
    AVDRMFrameDescriptor* result = nullptr;

    if ((Frame->m_pixFmt != AV_PIX_FMT_DRM_PRIME) || (Frame->m_type != FMT_DRMPRIME) ||
        !Frame->m_buffer || !Frame->m_priv[0])
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid DRM PRIME buffer %1 %2 %3 %4")
            .arg(Frame->m_buffer != nullptr).arg(Frame->m_priv[0] != nullptr)
            .arg(MythVideoFrame::FormatDescription(Frame->m_type),
                 av_get_pix_fmt_name(static_cast<AVPixelFormat>(Frame->m_pixFmt))));
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

    return  reinterpret_cast<AVDRMFrameDescriptor*>(Frame->m_buffer);
}

std::vector<MythVideoTextureOpenGL*>
MythDRMPRIMEInterop::Acquire(MythRenderOpenGL *Context,
                             MythVideoColourSpace *ColourSpace,
                             MythVideoFrame *Frame,
                             FrameScanType Scan)
{
    std::vector<MythVideoTextureOpenGL*> result;
    if (!Frame)
        return result;

    auto * drmdesc = VerifyBuffer(Context, Frame);
    if (!drmdesc)
        return result;

#ifdef USING_DRM_VIDEO
    if (HandleDRMVideo(ColourSpace, Frame, drmdesc))
        return result;
#endif

    bool firstpass  = m_openglTextures.isEmpty();
    bool interlaced = is_interlaced(Scan);
    bool composed   = static_cast<uint>(drmdesc->nb_layers) == 1 && m_composable;
    auto id         = reinterpret_cast<unsigned long long>(drmdesc);

    auto Separate = [this, id, drmdesc, Frame, firstpass, ColourSpace]()
    {
        std::vector<MythVideoTextureOpenGL*> textures;
        if (!m_openglTextures.contains(id))
        {
            textures = CreateTextures(drmdesc, m_openglContext, Frame, true);
            m_openglTextures.insert(id, textures);
        }
        else
        {
            textures = m_openglTextures[id];
        }

        if (textures.empty() ? false : MythVideoFrame::YUVFormat(textures[0]->m_frameFormat))
        {
            // Enable colour controls for YUV frame
            if (firstpass)
                ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
            ColourSpace->UpdateColourSpace(Frame);
            // and shader based deinterlacing
            Frame->m_deinterlaceAllowed = Frame->m_deinterlaceAllowed | DEINT_SHADER;
        }
        return textures;
    };

    // Separate texture for each plane
    if (!composed)
        return Separate();

    // Single RGB texture
    // Disable picture attributes
    if (firstpass)
        ColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);

    // Is deinterlacing selected? Accept any value as RGB frames can only be deinterlaced here
    bool doublerate = false;
    MythDeintType option = Frame->GetDoubleRateOption(DEINT_CPU | DEINT_SHADER | DEINT_DRIVER, DEINT_ALL);
    if (option != DEINT_NONE)
        doublerate = true;
    else
        option = Frame->GetSingleRateOption(DEINT_CPU | DEINT_SHADER | DEINT_DRIVER, DEINT_ALL);
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
        result = CreateTextures(drmdesc, m_openglContext, Frame, false,
                                m_deinterlacing ? kScan_Interlaced : kScan_Progressive);
        // Fallback to separate textures if the driver does not support composition
        if (result.empty())
        {
            m_composable = false;
            m_deinterlacing = false;
            DeleteTextures();
            LOG(VB_GENERAL, LOG_INFO, LOC + "YUV composition failed. Trying separate textures.");
            return Separate();
        }
        m_openglTextures.insert(id, result);
    }
    else
    {
        result = m_openglTextures[id];
    }

    if (m_deinterlacing)
    {
        result.clear();
        Frame->m_deinterlaceInuse = DEINT_DRIVER | DEINT_BASIC;
        Frame->m_deinterlaceInuse2x = doublerate;
        bool tff = Frame->m_interlacedReverse ? !Frame->m_topFieldFirst : Frame->m_topFieldFirst;
        int value {0};
        if (Scan == kScan_Interlaced)
            value = tff ? 1 : 0;
        else
            value = tff ? 0 : 1;
        result.emplace_back(m_openglTextures[id].at(value));
    }

    return result;
}

#ifdef USING_DRM_VIDEO
bool MythDRMPRIMEInterop::HandleDRMVideo(MythVideoColourSpace* ColourSpace, MythVideoFrame* Frame,
                                         AVDRMFrameDescriptor* DRMDesc)
{
    if (!((m_type == DRM_DRMPRIME) && !m_drmTriedAndFailed && Frame && DRMDesc && ColourSpace))
        return false;

    if (!m_drm)
        m_drm = new MythVideoDRM(ColourSpace);

    if (m_drm)
    {
        if (m_drm->IsValid())
        {
            ColourSpace->UpdateColourSpace(Frame);
            if (m_drm->RenderFrame(DRMDesc, Frame))
                return true;
        }

        // RenderFrame may have decided we should give up
        if (!m_drm->IsValid())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling DRM video");
            m_drmTriedAndFailed = true;
            delete m_drm;
            m_drm = nullptr;
        }
    }
    return false;
}
#endif
