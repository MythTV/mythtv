// MythTV
#include "videoout_opengl.h"
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
    if (Options.decoders->contains("mediacodec"))
        (*Options.safe_renderers)["mediacodec"].append(safe);
    if (Options.decoders->contains("vaapi2"))
        (*Options.safe_renderers)["vaapi2"].append(safe);
    if (Options.decoders->contains("nvdec"))
        (*Options.safe_renderers)["nvdec"].append(safe);

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
    memset(&m_pauseFrame, 0, sizeof(m_pauseFrame));
    m_pauseFrame.buf = nullptr;
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

bool VideoOutputOpenGL::CreateCPUResources(void)
{
    bool result = CreateBuffers();
    result &= CreatePauseFrame();
    return result;
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

    if (m_pauseFrame.buf)
    {
        av_freep(&m_pauseFrame.buf);
    }
    if (m_pauseFrame.qscale_table)
    {
        av_freep(&m_pauseFrame.qscale_table);
    }
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
    bool success = true;
    m_window = WinId;
    success &= VideoOutput::Init(VideoDim, VideoDispDim, Aspect, WinId, DisplayVisibleRect, CodecId);
    SetProfile();
    InitPictureAttributes();

    success &= CreateCPUResources();

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
        LOG(VB_GENERAL, LOG_ERR, LOC + "New video codec is not supported.");
        errorState = kError_Unknown;
        return false;
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
    if (Init(VideoDim, videoDispDim,
             Aspect, m_window, window.GetDisplayVisibleRect(), CodecId))
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
    OpenGLVideo::FrameType type = codec_sw_copy(video_codec_id) ? m_openGLVideoType : OpenGLVideo::kGLGPU;
    m_openGLVideo = new OpenGLVideo(m_render, &videoColourSpace, window.GetVideoDim(),
                                    window.GetVideoDispDim(), dvr, window.GetDisplayVideoRect(),
                                    window.GetVideoRect(), true, type);
    bool success = m_openGLVideo->IsValid();
    if (success)
    {
        // check if the profile changed
        if (codec_sw_copy(video_codec_id))
        {
            m_openGLVideoType    = m_openGLVideo->GetType();
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
    if (codec_is_mediacodec(video_codec_id))
        // vbuffers.Init(4, true, 1, 2, 2, 1);
        vbuffers.Init(8, true, 1, 4, 2, 1);
    else
        vbuffers.Init(31, true, 1, 12, 4, 2);
    return vbuffers.CreateBuffers(FMT_YV12,
                                  window.GetVideoDim().width(),
                                  window.GetVideoDim().height());
}

bool VideoOutputOpenGL::CreatePauseFrame(void)
{
    uint size = buffersize(FMT_YV12, vbuffers.GetScratchFrame()->width, vbuffers.GetScratchFrame()->height);
    unsigned char *buffer = static_cast<unsigned char*>(av_malloc(size));
    init(&m_pauseFrame, FMT_YV12, buffer, vbuffers.GetScratchFrame()->width,
         vbuffers.GetScratchFrame()->height, static_cast<int>(size));
    m_pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;
    if (!m_pauseFrame.buf)
        return false;
    clear(&m_pauseFrame);
    return true;
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

    bool pauseframe = false;
    if (!Frame)
    {
        Frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &m_pauseFrame);
        pauseframe = true;
    }

    bool dummy = Frame->dummy;
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

    if (m_openGLVideo && swframe && !dummy)
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
    if (Frame)
    {
        framesPlayed = Frame->frameNumber + 1;
        dummy = Frame->dummy;
    }

    if (!Frame)
    {
        Frame = vbuffers.GetScratchFrame();
        if (m_deinterlacing)
            Scan = kScan_Interlaced;
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

    // video
    if (m_openGLVideo && !dummy)
    {
        m_openGLVideo->SetVideoRects(vsz_enabled ? vsz_desired_display_rect :
                                                  window.GetDisplayVideoRect(),
                                    window.GetVideoRect());
        m_openGLVideo->PrepareFrame(Frame->top_field_first, Scan, m_stereo);
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
                (*it)->PrepareFrame(Frame->top_field_first, Scan,
                                    kStereoscopicModeNone, active);
                if (twopass)
                {
                    m_render->SetViewPort(second, true);
                    (*it)->PrepareFrame(Frame->top_field_first, Scan,
                                    kStereoscopicModeNone, active);
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
    QStringList list;
    if (!codec_sw_copy(CodecId) || getenv("NO_OPENGL"))
        return list;

    list << "opengl" << "opengl-yv12" << "opengl-hquyv";
    return list;
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
    VideoFrame *used = vbuffers.Head(kVideoBuffer_used);
    if (!used)
        used = vbuffers.GetScratchFrame();
    CopyFrame(&m_pauseFrame, used);
    DisplayTimecode = m_pauseFrame.disp_timecode;
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

int VideoOutputOpenGL::SetPictureAttribute(PictureAttribute Attribute, int Value)
{
    return VideoOutput::SetPictureAttribute(Attribute, Value);
}

bool VideoOutputOpenGL::SetupDeinterlace(bool Interlaced, const QString &OverrideFilter)
{
    if (!m_openGLVideo || !m_render)
        return false;

    OpenGLLocker ctx_lock(m_render);

    if (db_vdisp_profile)
        m_deintfiltername = db_vdisp_profile->GetFilteredDeint(OverrideFilter);

    if (MythCodecContext::isCodecDeinterlacer(m_deintfiltername))
        return false;

    if (!m_deintfiltername.contains("opengl"))
    {
        m_openGLVideo->SetDeinterlacing(false);
        VideoOutput::SetupDeinterlace(Interlaced, OverrideFilter);
        return m_deinterlacing;
    }

    // clear any non opengl filters
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
        if (m_deintfiltername.isEmpty() || m_deintfiltername.contains("opengl"))
        {
            return SetupDeinterlace(Enable);
        }
        else
        {
            // make sure opengl deinterlacing is disabled
            m_openGLVideo->SetDeinterlacing(false);
            if (!m_deintFiltMan || !m_deintFilter)
                return VideoOutput::SetupDeinterlace(Enable);
        }
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
    // anything OpenGL when using shaders
    if (Deinterlacer.contains("opengl") && (OpenGLVideo::kGLGPU != m_openGLVideoType))
        return true;

    // anything software based
    if (!Deinterlacer.contains("vdpau") && !Deinterlacer.contains("vaapi") && (OpenGLVideo::kGLGPU != m_openGLVideoType))
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
