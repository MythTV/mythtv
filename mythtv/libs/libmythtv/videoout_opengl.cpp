// MythTV
#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythplayer.h"
#include "videooutbase.h"
#include "videodisplayprofile.h"
#include "filtermanager.h"
#include "osd.h"
#include "mythuihelper.h"
#include "mythrender_opengl.h"
#include "mythpainter_ogl.h"
#include "mythcodeccontext.h"
#include "mythopenglinterop.h"
#include "videoout_opengl.h"

#define LOC QString("VidOutGL: ")

/*! \brief Generate the list of available OpenGL profiles
 *
 * \note This could be improved by eliminating unsupported profiles at run time -
 * but it is currently called statically and hence options would be fixed and unable
 * to reflect changes in UI render device.
*/
void VideoOutputOpenGL::GetRenderOptions(render_opts &Options,
                                         QStringList &SoftwareDeinterlacers)
{
    QStringList gldeints;
    gldeints << "opengllinearblend" <<
                "openglonefield" <<
                "openglkerneldeint" <<
                "openglbobdeint" <<
                "opengldoubleratelinearblend" <<
                "opengldoubleratekerneldeint" <<
                "opengldoubleratefieldorder";

    QStringList safe;
    safe << "opengl" << "opengl-yv12" << "opengl-hquyv";

    // all profiles can handle all software frames
    (*Options.safe_renderers)["dummy"].append(safe);
    (*Options.safe_renderers)["nuppel"].append(safe);
    if (Options.decoders->contains("ffmpeg"))
        (*Options.safe_renderers)["ffmpeg"].append(safe);
    if (Options.decoders->contains("openmax"))
        (*Options.safe_renderers)["openmax"].append(safe);
    if (Options.decoders->contains("mediacodec-dec"))
        (*Options.safe_renderers)["mediacodec-dec"].append(safe);
    if (Options.decoders->contains("vaapi-dec"))
        (*Options.safe_renderers)["vaapi-dec"].append(safe);
    if (Options.decoders->contains("nvdec-dec"))
        (*Options.safe_renderers)["nvdec-dec"].append(safe);
    if (Options.decoders->contains("vtb-dec"))
        (*Options.safe_renderers)["vtb-dec"].append(safe);

    // OpenGL UYVY
    Options.renderers->append("opengl");
    Options.deints->insert("opengl", SoftwareDeinterlacers + gldeints);
    (*Options.osds)["opengl"].append("opengl2");
    Options.priorities->insert("opengl", 65);

    // OpenGL HQ UYV
    Options.renderers->append("opengl-hquyv");
    Options.deints->insert("opengl-hquyv", SoftwareDeinterlacers + gldeints);
    (*Options.osds)["opengl-hquyv"].append("opengl2");
    Options.priorities->insert("opengl-hquyv", 60);

    // OpenGL YV12
    Options.renderers->append("opengl-yv12");
    Options.deints->insert("opengl-yv12", SoftwareDeinterlacers + gldeints);
    (*Options.osds)["opengl-yv12"].append("opengl2");
    Options.priorities->insert("opengl-yv12", 65);

#if defined(USING_VAAPI) || defined (USING_VTB) || defined (USING_MEDIACODEC)
    Options.renderers->append("opengl-hw");
    (*Options.deints)["opengl-hw"].append("none");
    (*Options.osds)["opengl-hw"].append("opengl2");
    (*Options.safe_renderers)["dummy"].append("opengl-hw");
    (*Options.safe_renderers)["nuppel"].append("opengl-hw");
    Options.priorities->insert("opengl-hw", 110);
#endif
#ifdef USING_VAAPI
    if (Options.decoders->contains("vaapi"))
        (*Options.safe_renderers)["vaapi"].append("opengl-hw");
#endif
#ifdef USING_VTB
    if (Options.decoders->contains("vtb"))
        (*Options.safe_renderers)["vtb"].append("opengl-hw");
#endif
#ifdef USING_MEDIACODEC
    if (Options.decoders->contains("mediacodec"))
        (*Options.safe_renderers)["mediacodec"].append("opengl-hw");
#endif
}

VideoOutputOpenGL::VideoOutputOpenGL(const QString &Profile)
  : VideoOutput(),
    m_render(nullptr),
    m_renderValid(true),
    m_openGLVideo(nullptr),
    m_openGLVideoPiPActive(nullptr),
    m_window(0),
    m_openGLPainter(nullptr),
    m_videoProfile(Profile),
    m_openGLVideoType(OpenGLVideo::StringToType(Profile))
{
    if (gCoreContext->GetBoolSetting("UseVideoModes", false))
        display_res = DisplayRes::GetDisplayRes(true);
}

VideoOutputOpenGL::~VideoOutputOpenGL()
{
    TearDown();
    if (m_render)
        m_render->DecrRef();
    m_render = nullptr;
}

void VideoOutputOpenGL::TearDown(void)
{
    DestroyCPUResources();
    DestroyVideoResources();
    DestroyGPUResources();
}

bool VideoOutputOpenGL::CreateGPUResources(void)
{
    bool result = SetupContext();
    if (result)
    {
        QSize size = window.GetVideoDim();
        InitDisplayMeasurements(size.width(), size.height(), false);
        CreatePainter();
    }
    return result;
}

bool VideoOutputOpenGL::CreateVideoResources(void)
{
    bool result = SetupOpenGL();
    MoveResize();
    return result;
}

void VideoOutputOpenGL::DestroyCPUResources(void)
{
    DiscardFrames(true);
    vbuffers.DeleteBuffers();
    vbuffers.Reset();
}

void VideoOutputOpenGL::DestroyGPUResources(void)
{
    if (m_render)
        m_render->makeCurrent();
    if (m_openGLPainter)
        m_openGLPainter->SetSwapControl(true);
    m_openGLPainter = nullptr;
    if (m_render)
        m_render->doneCurrent();
}

void VideoOutputOpenGL::DestroyVideoResources(void)
{
    if (m_render)
        m_render->makeCurrent();

    if (m_openGLVideo)
    {
        delete m_openGLVideo;
        m_openGLVideo = nullptr;
    }

    while (!m_openGLVideoPiPs.empty())
    {
        delete *m_openGLVideoPiPs.begin();
        m_openGLVideoPiPs.erase(m_openGLVideoPiPs.begin());
    }
    m_openGLVideoPiPsReady.clear();

    if (m_render)
        m_render->doneCurrent();
}

bool VideoOutputOpenGL::Init(const QSize &VideoDim, const QSize &VideoDispDim, float Aspect,
                             WId WinId, const QRect &DisplayVisibleRect, MythCodecID CodecId)
{
    // if hardware initialisation failed, fallback to yv12
    if ((OpenGLVideo::kGLInterop == m_openGLVideoType) && codec_sw_copy(CodecId))
        m_openGLVideoType = OpenGLVideo::kGLYV12;

    bool success = true;
    m_window = WinId;
    success &= VideoOutput::Init(VideoDim, VideoDispDim, Aspect, WinId, DisplayVisibleRect, CodecId);
    SetProfile();
    InitPictureAttributes();

    success &= CreateBuffers();

    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "Deferring creation of OpenGL resources");
        m_renderValid = false;
    }
    else
    {
        success &= CreateGPUResources();
        success &= CreateVideoResources();
    }

    if (!success)
        TearDown();
    return success;
}

void VideoOutputOpenGL::SetProfile(void)
{
    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer(m_videoProfile);
}

bool VideoOutputOpenGL::InputChanged(const QSize &VideoDim, const QSize &videoDispDim,
                                     float Aspect, MythCodecID CodecId, bool &AspectOnly)
{
    QSize currentvideodim     = window.GetVideoDim();
    QSize currentvideodispdim = window.GetVideoDispDim();

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Video changed: %1x%2 (%3x%4) '%5' (Aspect %6)"
                                             "-> %7x%8 (%9x%10) '%11' (Aspect %12)")
        .arg(currentvideodispdim.width()).arg(currentvideodispdim.height())
        .arg(currentvideodim.width()).arg(currentvideodim.height())
        .arg(toString(video_codec_id)).arg(static_cast<double>(window.GetVideoAspect()))
        .arg(videoDispDim.width()).arg(videoDispDim.height())
        .arg(VideoDim.width()).arg(VideoDim.height())
        .arg(toString(CodecId)).arg(static_cast<double>(Aspect)));

    bool cidchanged = (CodecId != video_codec_id);
    bool reschanged = (videoDispDim != currentvideodispdim);

    // aspect ratio changes are a no-op as changes are handled at display time
    if (!cidchanged && !reschanged)
    {
        AspectOnly = true;
        return true;
    }

    // fail fast if we don't know how to display the codec
    if (!codec_sw_copy(CodecId))
    {
        if (!gCoreContext->IsUIThread())
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Cannot check interop support from this thread");
        }
        else if (MythOpenGLInterop::GetInteropType(CodecId) == MythOpenGLInterop::Unsupported)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "New video codec is not supported.");
            errorState = kError_Unknown;
            return false;
        }
    }

    // Ensure we don't lose embedding through program changes.
    bool wasembedding = window.IsEmbedding();
    QRect oldrect;
    if (wasembedding)
    {
        oldrect = window.GetEmbeddingRect();
        StopEmbedding();
    }

    // Destroy buffers
    if (gCoreContext->IsUIThread())
        TearDown();
    else
        DestroyCPUResources();

    // Recreate buffers
    if (Init(VideoDim, videoDispDim, Aspect, m_window, window.GetDisplayVisibleRect(), CodecId))
    {
        if (wasembedding)
            EmbedInWidget(oldrect);
        if (gCoreContext->IsUIThread())
            BestDeint();
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to re-initialise video output.");
    errorState = kError_Unknown;
    return false;
}

bool VideoOutputOpenGL::SetupContext(void)
{
    if (m_render)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Re-using context"));
        return true;
    }

    MythMainWindow* win = MythMainWindow::getMainWindow();
    if (!win)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get MythMainWindow");
        return false;
    }

    m_render = dynamic_cast<MythRenderOpenGL*>(win->GetRenderDevice());
    if (m_render)
    {
        m_render->IncrRef();
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Using main UI render context");
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to use OpenGL without OpenGL UI");
    return false;
}

bool VideoOutputOpenGL::SetupOpenGL(void)
{
    if (!m_render)
        return false;

    QRect dvr = window.GetDisplayVisibleRect();

    MythMainWindow *mainWin = GetMythMainWindow();
    QSize mainSize = mainWin->size();

    // If the Video screen mode has vertically less pixels
    // than the GUI screen mode - OpenGL coordinate adjustments
    // must be made to put the video at the top of the display 
    // area instead of at the bottom.
    if (dvr.height() < mainSize.height())
        dvr.setTop(dvr.top()-mainSize.height()+dvr.height());

    // If the Video screen mode has horizontally less pixels
    // than the GUI screen mode - OpenGL width must be set
    // as the higher GUI width so that the Program Guide
    // invoked from playback is not cut off.
    if (dvr.width() < mainSize.width())
        dvr.setWidth(mainSize.width());

    if (video_codec_id == kCodec_NONE)
    {
        m_render->SetViewPort(QRect(QPoint(),dvr.size()));
        return true;
    }

    if (window.GetPIPState() >= kPIPStandAlone)
    {
        QRect tmprect = QRect(QPoint(0,0), dvr.size());
        ResizeDisplayWindow(tmprect, true);
    }

    OpenGLLocker ctx_lock(m_render);
    m_openGLVideo = new OpenGLVideo(m_render, &videoColourSpace, window.GetVideoDim(),
                                    window.GetVideoDispDim(), dvr, window.GetDisplayVideoRect(),
                                    window.GetVideoRect(), true, m_openGLVideoType);
    bool success = m_openGLVideo->IsValid();
    if (success)
    {
        // check if the profile changed
        if (codec_sw_copy(video_codec_id))
        {
            m_openGLVideoType = m_openGLVideo->GetType();
            m_videoProfile = OpenGLVideo::TypeToString(m_openGLVideoType);
        }

        bool temp_deinterlacing = m_deinterlacing;
        SetDeinterlacingEnabled(true);
        if (!temp_deinterlacing)
            SetDeinterlacingEnabled(false);
    }

    return success;
}

void VideoOutputOpenGL::CreatePainter(void)
{
    MythMainWindow *win = MythMainWindow::getMainWindow();
    m_openGLPainter = dynamic_cast<MythOpenGLPainter*>(win->GetCurrentPainter());
    if (!m_openGLPainter)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get painter");
        return;
    }
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Using main UI painter");
    m_openGLPainter->SetSwapControl(false);
}

bool VideoOutputOpenGL::CreateBuffers(void)
{
    if (codec_is_mediacodec_dec(video_codec_id))
    {
        vbuffers.Init(8, false, 1, 4, 2, 1);
    }
    else if (codec_is_mediacodec(video_codec_id))
    {
        uint size = VideoBuffers::GetNumBuffers(FMT_MEDIACODEC);
        vbuffers.Init(size, false, 1, 2, 2, 1);
        bool success = true;
        for (uint i = 0; i < size; i++)
            success &= vbuffers.CreateBuffer(window.GetVideoDim().width(), window.GetVideoDim().height(), i, nullptr, FMT_MEDIACODEC);
        return success;
    }
    else if (codec_is_vaapi(video_codec_id))
    {
        uint size = VideoBuffers::GetNumBuffers(FMT_VAAPI);
        vbuffers.Init(size, false, 2, 1, 4, 1);
        bool success = true;
        for (uint i = 0; i < size; i++)
            success &= vbuffers.CreateBuffer(window.GetVideoDim().width(), window.GetVideoDim().height(), i, nullptr, FMT_VAAPI);
        return success;
    }
    else if (codec_is_vtb_dec(video_codec_id))
    {
        vbuffers.Init(VideoBuffers::GetNumBuffers(FMT_VTB), false, 1, 4, 2, 1);
    }
    else if (codec_is_vtb(video_codec_id))
    {
        uint size = VideoBuffers::GetNumBuffers(FMT_VTB);
        vbuffers.Init(size, false, 1, 4, 2, 1);
        bool success = true;
        for (uint i = 0; i < size; ++i)
            success &= vbuffers.CreateBuffer(window.GetVideoDim().width(), window.GetVideoDim().height(), i, nullptr, FMT_VTB);
        return success;
    }
    else
    {
        vbuffers.Init(VideoBuffers::GetNumBuffers(FMT_YV12), false, 1, 12, 4, 2);
    }

    return vbuffers.CreateBuffers(FMT_YV12, window.GetVideoDim().width(), window.GetVideoDim().height());
}

void VideoOutputOpenGL::ProcessFrame(VideoFrame *Frame, OSD */*osd*/,
                                     FilterChain *FilterList,const PIPMap &PiPPlayers,
                                     FrameScanType Scan)
{
    if (!m_render)
        return;

    if (!m_renderValid)
    {
        if (!gCoreContext->IsUIThread())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "ProcessFrame called from wrong thread");
        }
        QSize size = window.GetVideoDim();
        InitDisplayMeasurements(size.width(), size.height(), false);
        DestroyVideoResources();
        CreateVideoResources();
        BestDeint();
        m_renderValid = true;
    }

    bool swframe   = codec_sw_copy(video_codec_id) && video_codec_id != kCodec_NONE;
    bool deintproc = m_deinterlacing && (m_deintFilter != nullptr);
    OpenGLLocker ctx_lock(m_render);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PROCESS_FRAME_START");

    bool pauseframe = !Frame;
    bool dummy = Frame ? Frame->dummy : false;
    if (FilterList && swframe && !dummy)
        FilterList->ProcessFrame(Frame);

    if (swframe && deintproc && m_deinterlaceBeforeOSD && !pauseframe && !dummy)
        m_deintFilter->ProcessFrame(Frame, Scan);

    if (!window.IsEmbedding())
    {
        m_openGLVideoPiPActive = nullptr;
        ShowPIPs(Frame, PiPPlayers);
    }

    if (swframe && deintproc && !m_deinterlaceBeforeOSD && !pauseframe && !dummy)
        m_deintFilter->ProcessFrame(Frame, Scan);

    if (m_openGLVideo && swframe && !dummy && !pauseframe)
        m_openGLVideo->UpdateInputFrame(Frame);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PROCESS_FRAME_END");
}

void VideoOutputOpenGL::PrepareFrame(VideoFrame *Frame, FrameScanType Scan, OSD *Osd)
{
    if (!m_render)
        return;

    OpenGLLocker ctx_lock(m_render);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREPARE_FRAME_START");

    bool dummy = false;
    int topfieldfirst = 0;
    if (Frame)
    {
        framesPlayed = Frame->frameNumber + 1;
        topfieldfirst = Frame->top_field_first;
        dummy = Frame->dummy;
    }

    m_render->BindFramebuffer(nullptr);
    if (db_letterbox_colour == kLetterBoxColour_Gray25)
        m_render->SetBackground(127, 127, 127, 255);
    else
        m_render->SetBackground(0, 0, 0, 255);
    m_render->ClearFramebuffer();

    // stereoscopic views
    QRect main   = m_render->GetViewPort();
    QRect first  = main;
    QRect second = main;
    bool twopass = (m_stereo == kStereoscopicModeSideBySide) || (m_stereo == kStereoscopicModeTopAndBottom);

    if (kStereoscopicModeSideBySide == m_stereo)
    {
        first  = QRect(main.left() / 2,  main.top(), main.width() / 2, main.height());
        second = first.translated(main.width() / 2, 0);
    }
    else if (kStereoscopicModeTopAndBottom == m_stereo)
    {
        first  = QRect(main.left(),  main.top() / 2, main.width(), main.height() / 2);
        second = first.translated(0, main.height() / 2);
    }

    // main UI when embedded
    if (window.IsEmbedding())
    {
        MythMainWindow *window = GetMythMainWindow();
        if (window && window->GetPaintWindow())
        {
            if (twopass)
                m_render->SetViewPort(first, true);
            window->GetPaintWindow()->clearMask();
            window->draw(m_openGLPainter);
            if (twopass)
            {
                m_render->SetViewPort(second, true);
                window->GetPaintWindow()->clearMask();
                window->draw(m_openGLPainter);
                m_render->SetViewPort(main, true);
            }
        }
    }

    // video
    if (m_openGLVideo && !dummy)
    {
        m_openGLVideo->SetVideoRects(vsz_enabled ? vsz_desired_display_rect :
                                                  window.GetDisplayVideoRect(),
                                    window.GetVideoRect());
        m_openGLVideo->PrepareFrame(Frame, topfieldfirst, Scan, m_stereo);
    }

    // PiPs/PBPs
    if (m_openGLVideoPiPs.size())
    {
        QMap<MythPlayer*,OpenGLVideo*>::iterator it = m_openGLVideoPiPs.begin();
        for (; it != m_openGLVideoPiPs.end(); ++it)
        {
            if (m_openGLVideoPiPsReady[it.key()])
            {
                bool active = m_openGLVideoPiPActive == *it;
                if (twopass)
                    m_render->SetViewPort(first, true);
                (*it)->PrepareFrame(nullptr, topfieldfirst, Scan, kStereoscopicModeNone, active);
                if (twopass)
                {
                    m_render->SetViewPort(second, true);
                    (*it)->PrepareFrame(nullptr, topfieldfirst, Scan, kStereoscopicModeNone, active);
                    m_render->SetViewPort(main);
                }
            }
        }
    }

    // visualisation
    if (m_visual && m_openGLPainter && !window.IsEmbedding())
    {
        if (twopass)
            m_render->SetViewPort(first, true);
        m_visual->Draw(GetTotalOSDBounds(), m_openGLPainter, nullptr);
        if (twopass)
        {
            m_render->SetViewPort(second, true);
            m_visual->Draw(GetTotalOSDBounds(), m_openGLPainter, nullptr);
            m_render->SetViewPort(main);
        }
    }

    // OSD
    if (Osd && m_openGLPainter && !window.IsEmbedding())
    {
        if (twopass)
            m_render->SetViewPort(first, true);
        Osd->DrawDirect(m_openGLPainter, GetTotalOSDBounds().size(), true);
        if (twopass)
        {
            m_render->SetViewPort(second, true);
            Osd->DrawDirect(m_openGLPainter, GetTotalOSDBounds().size(), true);
            m_render->SetViewPort(main);
        }
    }

    m_render->Flush(false);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREPARE_FRAME_END");
}

void VideoOutputOpenGL::Show(FrameScanType /*scan*/)
{
    if (m_render && !IsErrored())
    {
        m_render->makeCurrent();
        if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
            m_render->logDebugMarker(LOC + "SHOW");
        m_render->swapBuffers();
        m_render->doneCurrent();
    }
}

/*! \brief Generate a list of supported OpenGL profiles.
 *
 * \note This list could be filtered based upon current feature support. This
 * would however assume an OpenGL render device (not currently a given) but more
 * importantly, filtering out a selected profile encourages the display profile
 * code to use a higher priority, non-OpenGL renderer (such as VDPAU). By not
 * filtering, we allow the OpenGL video code to fallback to a supported, reasonable
 * alternative.
*/
QStringList VideoOutputOpenGL::GetAllowedRenderers(MythCodecID CodecId, const QSize&)
{
    QStringList allowed;
    if (getenv("NO_OPENGL"))
        return allowed;

    if (codec_sw_copy(CodecId))
    {
        allowed << "opengl" << "opengl-yv12" << "opengl-hquyv";
        return allowed;
    }

    allowed += MythOpenGLInterop::GetAllowedRenderers(CodecId);
    return allowed;
}

void VideoOutputOpenGL::Zoom(ZoomDirection Direction)
{
    VideoOutput::Zoom(Direction);
    MoveResize();
}

void VideoOutputOpenGL::MoveResize(void)
{
    VideoOutput::MoveResize();
    if (m_openGLVideo)
    {
        m_openGLVideo->SetVideoRects(vsz_enabled ? vsz_desired_display_rect : window.GetDisplayVideoRect(),
                                     window.GetVideoRect());
    }
}

void VideoOutputOpenGL::UpdatePauseFrame(int64_t &DisplayTimecode)
{
    vbuffers.begin_lock(kVideoBuffer_used);
    VideoFrame *used = vbuffers.Head(kVideoBuffer_used);
    if (used)
    {
        m_openGLVideo->UpdateInputFrame(used);
        DisplayTimecode = used->disp_timecode;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC + "Could not update pause frame");
    }
    vbuffers.end_lock();
}

void VideoOutputOpenGL::InitPictureAttributes(void)
{
    videoColourSpace.SetSupportedAttributes(static_cast<PictureAttributeSupported>
                                       (kPictureAttributeSupported_Brightness |
                                        kPictureAttributeSupported_Contrast |
                                        kPictureAttributeSupported_Colour |
                                        kPictureAttributeSupported_Hue |
                                        kPictureAttributeSupported_StudioLevels));
}

bool VideoOutputOpenGL::SetupDeinterlace(bool Interlaced, const QString &OverrideFilter)
{
    if (!m_openGLVideo || !m_render)
        return false;

    OpenGLLocker ctx_lock(m_render);

    if (db_vdisp_profile)
        m_deintfiltername = db_vdisp_profile->GetFilteredDeint(OverrideFilter);

    if (!m_deintfiltername.contains("opengl") && !m_deintfiltername.contains("vaapi"))
    {
        m_openGLVideo->SetDeinterlacing(false);
        VideoOutput::SetupDeinterlace(Interlaced, OverrideFilter);
        return m_deinterlacing;
    }

    // clear any non hardware filters
    if (m_deintFiltMan)
    {
        delete m_deintFiltMan;
        m_deintFiltMan = nullptr;
    }
    if (m_deintFilter)
    {
        delete m_deintFilter;
        m_deintFilter = nullptr;
    }

    if (OpenGLVideo::kGLInterop == m_openGLVideoType)
    {
        m_deinterlacing = m_deintfiltername.contains("vaapi") ? Interlaced : false;
        return m_deinterlacing;
    }

    m_deinterlacing = Interlaced;
    if (m_deinterlacing && !m_deintfiltername.isEmpty())
    {
        if (!m_openGLVideo->AddDeinterlacer(m_deintfiltername))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Couldn't load deinterlace filter %1").arg(m_deintfiltername));
            m_deinterlacing = false;
            m_deintfiltername = "";
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using deinterlace method %1").arg(m_deintfiltername));
        }
    }

    m_openGLVideo->SetDeinterlacing(m_deinterlacing);
    return m_deinterlacing;
}

bool VideoOutputOpenGL::SetDeinterlacingEnabled(bool Enable)
{
    if (!m_openGLVideo || !m_render)
        return false;

    OpenGLLocker ctx_lock(m_render);

    if (Enable)
    {
        if (!m_deintfiltername.contains("opengl"))
            m_openGLVideo->SetDeinterlacing(false);
        return SetupDeinterlace(Enable);
    }

    m_openGLVideo->SetDeinterlacing(Enable);
    m_deinterlacing = Enable;
    return m_deinterlacing;
}

void VideoOutputOpenGL::ShowPIP(VideoFrame*, MythPlayer *PiPPlayer, PIPLocation Location)
{
    if (!PiPPlayer)
        return;

    int pipw, piph;
    VideoFrame *pipimage     = PiPPlayer->GetCurrentFrame(pipw, piph);
    const QSize pipvideodim  = PiPPlayer->GetVideoBufferSize();
    QRect       pipvideorect = QRect(QPoint(0, 0), pipvideodim);

    if ((PiPPlayer->GetVideoAspect() <= 0.0f) || !pipimage || !pipimage->buf ||
        (pipimage->codec != FMT_YV12) || !PiPPlayer->IsPIPVisible())
    {
        PiPPlayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(Location, PiPPlayer);
    QRect dvr = window.GetDisplayVisibleRect();

    m_openGLVideoPiPsReady[PiPPlayer] = false;
    OpenGLVideo *gl_pipchain = m_openGLVideoPiPs[PiPPlayer];
    if (!gl_pipchain)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Initialise PiP");
        VideoColourSpace *colourspace = new VideoColourSpace(&videoColourSpace);
        m_openGLVideoPiPs[PiPPlayer] = gl_pipchain = new OpenGLVideo(m_render, colourspace,
                                                                pipvideodim, pipvideodim,
                                                                dvr, position, pipvideorect,
                                                                false, m_openGLVideoType);

        colourspace->DecrRef();
        if (!gl_pipchain->IsValid())
        {
            PiPPlayer->ReleaseCurrentFrame(pipimage);
            return;
        }
        gl_pipchain->SetMasterViewport(dvr.size());
    }

    if (gl_pipchain->GetVideoSize() != pipvideodim)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Re-initialise PiP.");
        delete gl_pipchain;
        VideoColourSpace *colourspace = new VideoColourSpace(&videoColourSpace);
        m_openGLVideoPiPs[PiPPlayer] = gl_pipchain = new OpenGLVideo(m_render, colourspace,
                                                                pipvideodim, pipvideodim,
                                                                dvr, position, pipvideorect,
                                                                false, m_openGLVideoType);
        colourspace->DecrRef();
        if (!gl_pipchain->IsValid())
        {
            PiPPlayer->ReleaseCurrentFrame(pipimage);
            return;
        }
        gl_pipchain->SetMasterViewport(dvr.size());
    }

    if (gl_pipchain->IsValid())
    {
        gl_pipchain->SetVideoRects(position, pipvideorect);
        gl_pipchain->UpdateInputFrame(pipimage);
    }

    m_openGLVideoPiPsReady[PiPPlayer] = true;
    if (PiPPlayer->IsPIPActive())
        m_openGLVideoPiPActive = gl_pipchain;
    PiPPlayer->ReleaseCurrentFrame(pipimage);
}

void VideoOutputOpenGL::RemovePIP(MythPlayer *PiPPlayer)
{
    if (m_openGLVideoPiPs.contains(PiPPlayer))
    {
        m_render->makeCurrent();
        delete m_openGLVideoPiPs.take(PiPPlayer);
        m_openGLVideoPiPsReady.remove(PiPPlayer);
        m_openGLVideoPiPs.remove(PiPPlayer);
        m_render->doneCurrent();
    }
}

void VideoOutputOpenGL::MoveResizeWindow(QRect NewRect)
{
    if (m_render)
        m_render->MoveResizeWindow(NewRect);
}

void VideoOutputOpenGL::EmbedInWidget(const QRect &Rect)
{
    VideoOutput::EmbedInWidget(Rect);
    MoveResize();
}

void VideoOutputOpenGL::StopEmbedding(void)
{
    VideoOutput::StopEmbedding();
    MoveResize();
}

bool VideoOutputOpenGL::ApproveDeintFilter(const QString &Deinterlacer) const
{
    bool vaapi = OpenGLVideo::kGLInterop == m_openGLVideoType;
    // anything OpenGL when using shaders
    if (!vaapi && Deinterlacer.contains("opengl"))
        return true;

    // vaapi if allowed
    if (vaapi && Deinterlacer.contains("vaapi"))
        return true;

    // anything software based
    if (!vaapi && !Deinterlacer.contains("vdpau") && !Deinterlacer.contains("vaapi"))
        return true;

    return VideoOutput::ApproveDeintFilter(Deinterlacer);
}

QStringList VideoOutputOpenGL::GetVisualiserList(void)
{
    if (m_render)
        return VideoVisual::GetVisualiserList(m_render->Type());
    return VideoOutput::GetVisualiserList();
}

MythPainter *VideoOutputOpenGL::GetOSDPainter(void)
{
    return m_openGLPainter;
}

bool VideoOutputOpenGL::CanVisualise(AudioPlayer *Audio, MythRender*)
{
    return VideoOutput::CanVisualise(Audio, m_render);
}

bool VideoOutputOpenGL::SetupVisualisation(AudioPlayer *Audio, MythRender*, const QString &Name)
{
    return VideoOutput::SetupVisualisation(Audio, m_render, Name);
}
