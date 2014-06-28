#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythplayer.h"
#include "videooutbase.h"
#include "videoout_opengl.h"
#include "videodisplayprofile.h"
#include "filtermanager.h"
#include "osd.h"
#include "mythuihelper.h"

#define LOC      QString("VidOutGL: ")

void VideoOutputOpenGL::GetRenderOptions(render_opts &opts,
                                         QStringList &cpudeints)
{
    // full featured profile
    opts.renderers->append("opengl");
    opts.deints->insert("opengl", cpudeints);
    (*opts.deints)["opengl"].append("opengllinearblend");
    (*opts.deints)["opengl"].append("openglonefield");
    (*opts.deints)["opengl"].append("openglkerneldeint");
    (*opts.deints)["opengl"].append("bobdeint");
    (*opts.deints)["opengl"].append("openglbobdeint");
    (*opts.deints)["opengl"].append("opengldoubleratelinearblend");
    (*opts.deints)["opengl"].append("opengldoubleratekerneldeint");
    (*opts.deints)["opengl"].append("opengldoubleratefieldorder");
    (*opts.osds)["opengl"].append("opengl2");
    (*opts.safe_renderers)["dummy"].append("opengl");
    (*opts.safe_renderers)["nuppel"].append("opengl");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("opengl");
    if (opts.decoders->contains("vda"))
        (*opts.safe_renderers)["vda"].append("opengl");
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append("opengl");
    opts.priorities->insert("opengl", 65);

    // lite profile - no colourspace control, GPU deinterlacing
    opts.renderers->append("opengl-lite");
    opts.deints->insert("opengl-lite", cpudeints);
    (*opts.deints)["opengl-lite"].append("bobdeint");
    (*opts.osds)["opengl-lite"].append("opengl2");
    (*opts.safe_renderers)["dummy"].append("opengl-lite");
    (*opts.safe_renderers)["nuppel"].append("opengl-lite");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("opengl-lite");
    if (opts.decoders->contains("vda"))
        (*opts.safe_renderers)["vda"].append("opengl-lite");
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append("opengl-lite");
    opts.priorities->insert("opengl", 60);
}

VideoOutputOpenGL::VideoOutputOpenGL(const QString &profile)
    : VideoOutput(),
    gl_context_lock(QMutex::Recursive), gl_context(NULL), gl_valid(true),
    gl_videochain(NULL), gl_pipchain_active(NULL),
    gl_parent_win(0),    gl_painter(NULL), gl_created_painter(false),
    gl_opengl_lite(false)
{
    if (profile.contains("lite"))
        gl_opengl_lite = true;

    memset(&av_pause_frame, 0, sizeof(av_pause_frame));
    av_pause_frame.buf = NULL;

    if (gCoreContext->GetNumSetting("UseVideoModes", 0))
        display_res = DisplayRes::GetDisplayRes(true);
}

VideoOutputOpenGL::~VideoOutputOpenGL()
{
    gl_context_lock.lock();
    TearDown();

    if (gl_context)
        gl_context->DecrRef();
    gl_context = NULL;
    gl_context_lock.unlock();
}

void VideoOutputOpenGL::TearDown(void)
{
    gl_context_lock.lock();
    DestroyCPUResources();
    DestroyVideoResources();
    DestroyGPUResources();
    gl_context_lock.unlock();
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
    QSize size = window.GetActualVideoDim();
    InitDisplayMeasurements(size.width(), size.height(), false);
    CreatePainter();
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
    gl_context_lock.lock();
    DiscardFrames(true);
    vbuffers.DeleteBuffers();
    vbuffers.Reset();

    if (av_pause_frame.buf)
    {
        delete [] av_pause_frame.buf;
        av_pause_frame.buf = NULL;
    }
    if (av_pause_frame.qscale_table)
    {
        delete [] av_pause_frame.qscale_table;
        av_pause_frame.qscale_table = NULL;
    }
    gl_context_lock.unlock();
}

void VideoOutputOpenGL::DestroyGPUResources(void)
{
    gl_context_lock.lock();
    if (gl_context)
        gl_context->makeCurrent();

    if (gl_created_painter)
        delete gl_painter;
    else if (gl_painter)
        gl_painter->SetSwapControl(true);

    gl_painter = NULL;
    gl_created_painter = false;

    if (gl_context)
        gl_context->doneCurrent();
    gl_context_lock.unlock();
}

void VideoOutputOpenGL::DestroyVideoResources(void)
{
    gl_context_lock.lock();
    if (gl_context)
        gl_context->makeCurrent();

    if (gl_videochain)
    {
        delete gl_videochain;
        gl_videochain = NULL;
    }

    while (!gl_pipchains.empty())
    {
        delete *gl_pipchains.begin();
        gl_pipchains.erase(gl_pipchains.begin());
    }
    gl_pip_ready.clear();

    if (gl_context)
        gl_context->doneCurrent();
    gl_context_lock.unlock();
}

bool VideoOutputOpenGL::Init(const QSize &video_dim_buf,
                             const QSize &video_dim_disp,
                             float aspect, WId winid,
                             const QRect &win_rect, MythCodecID codec_id)
{
    QMutexLocker locker(&gl_context_lock);
    bool success = true;
    window.SetAllowPreviewEPG(true);
    gl_parent_win = winid;
    success &= VideoOutput::Init(video_dim_buf, video_dim_disp,
                                 aspect, winid,
                                 win_rect, codec_id);
    SetProfile();
    InitPictureAttributes();

    success &= CreateCPUResources();

    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            "Deferring creation of OpenGL resources");
        gl_valid = false;
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
    {
        db_vdisp_profile->SetVideoRenderer(
                    gl_opengl_lite ? "opengl-lite" : "opengl");
    }
}

bool VideoOutputOpenGL::InputChanged(const QSize &video_dim_buf,
                                     const QSize &video_dim_disp,
                                     float        aspect,
                                     MythCodecID  av_codec_id,
                                     void        *codec_private,
                                     bool        &aspect_only)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("InputChanged(%1,%2,%3) %4->%5")
            .arg(video_dim_disp.width()).arg(video_dim_disp.height())
            .arg(aspect)
            .arg(toString(video_codec_id)).arg(toString(av_codec_id)));

    QMutexLocker locker(&gl_context_lock);

    // Ensure we don't lose embedding through program changes. This duplicates
    // code in VideoOutput::Init but we need start here otherwise the embedding
    // is lost during window re-initialistion.
    bool wasembedding = window.IsEmbedding();
    QRect oldrect;
    if (wasembedding)
    {
        oldrect = window.GetEmbeddingRect();
        StopEmbedding();
    }

    if (!codec_is_std(av_codec_id))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "New video codec is not supported.");
        errorState = kError_Unknown;
        return false;
    }

    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = video_dim_disp != window.GetActualVideoDim();
    bool asp_changed = aspect      != window.GetVideoAspect();

    if (!res_changed && !cid_changed)
    {
        if (asp_changed)
        {
            aspect_only = true;
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        if (wasembedding)
            EmbedInWidget(oldrect);
        return true;
    }

    if (gCoreContext->IsUIThread())
        TearDown();
    else
        DestroyCPUResources();

    QRect disp = window.GetDisplayVisibleRect();
    if (Init(video_dim_buf, video_dim_disp,
             aspect, gl_parent_win, disp, av_codec_id))
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
    QMutexLocker locker(&gl_context_lock);

    if (gl_context)
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

    gl_context = dynamic_cast<MythRenderOpenGL*>(win->GetRenderDevice());
    if (gl_context)
    {
        gl_context->IncrRef();
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Using main UI render context");
        return true;
    }

    QGLWidget *device = (QGLWidget*)QWidget::find(gl_parent_win);
    if (!device)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to cast parent to QGLWidget");
        return false;
    }

    gl_context = MythRenderOpenGL::Create(QString(), device);
    if (gl_context && gl_context->create())
    {
        gl_context->Init();
        LOG(VB_GENERAL, LOG_INFO, LOC + "Created MythRenderOpenGL device.");
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create MythRenderOpenGL device.");
    if (gl_context)
        gl_context->DecrRef();
    gl_context = NULL;
    return false;
}

bool VideoOutputOpenGL::SetupOpenGL(void)
{
    if (!gl_context)
        return false;

    const QRect dvr = window.GetDisplayVisibleRect();

    if (video_codec_id == kCodec_NONE)
    {
        gl_context->SetViewPort(QRect(QPoint(),dvr.size()));
        return true;
    }

    if (window.GetPIPState() >= kPIPStandAlone)
    {
        QRect tmprect = QRect(QPoint(0,0), dvr.size());
        ResizeDisplayWindow(tmprect, true);
    }
    bool success = false;
    OpenGLLocker ctx_lock(gl_context);
    gl_videochain = new OpenGLVideo();
    QString options = GetFilters();
    if (gl_opengl_lite)
        options += " preferycbcr";
    success = gl_videochain->Init(gl_context, &videoColourSpace,
                                  window.GetVideoDim(),
                                  window.GetVideoDispDim(), dvr,
                                  window.GetDisplayVideoRect(),
                                  window.GetVideoRect(), true,
                                  options, !codec_is_std(video_codec_id));
    if (success)
    {
        bool temp_deinterlacing = m_deinterlacing;
        if (!m_deintfiltername.isEmpty() &&
            !m_deintfiltername.contains("opengl"))
        {
            gl_videochain->SetSoftwareDeinterlacer(m_deintfiltername);
        }
        SetDeinterlacingEnabled(true);
        if (!temp_deinterlacing)
        {
            SetDeinterlacingEnabled(false);
        }
    }

    return success;
}

void VideoOutputOpenGL::CreatePainter(void)
{
    QMutexLocker locker(&gl_context_lock);

    gl_created_painter = false;
    MythMainWindow *win = MythMainWindow::getMainWindow();
    if (gl_context && !gl_context->IsShared())
    {
        QGLWidget *device = (QGLWidget*)QWidget::find(gl_parent_win);
        gl_painter = new MythOpenGLPainter(gl_context, device);
        if (!gl_painter)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create painter");
            return;
        }
        gl_created_painter = true;
    }
    else
    {
        gl_painter = (MythOpenGLPainter*)win->GetCurrentPainter();
        if (!gl_painter)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get painter");
            return;
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Using main UI painter");
    }
    gl_painter->SetSwapControl(false);
}

bool VideoOutputOpenGL::CreateBuffers(void)
{
    QMutexLocker locker(&gl_context_lock);
    vbuffers.Init(31, true, 1, 12, 4, 2);
    return vbuffers.CreateBuffers(FMT_YV12,
                                  window.GetVideoDim().width(),
                                  window.GetVideoDim().height());
}

bool VideoOutputOpenGL::CreatePauseFrame(void)
{
    init(&av_pause_frame, FMT_YV12,
         new unsigned char[vbuffers.GetScratchFrame()->size + 128],
         vbuffers.GetScratchFrame()->width,
         vbuffers.GetScratchFrame()->height,
         vbuffers.GetScratchFrame()->size);

    av_pause_frame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    if (!av_pause_frame.buf)
        return false;

    clear(&av_pause_frame);
    return true;
}

void VideoOutputOpenGL::ProcessFrame(VideoFrame *frame, OSD *osd,
                                     FilterChain *filterList,
                                     const PIPMap &pipPlayers,
                                     FrameScanType scan)
{
    QMutexLocker locker(&gl_context_lock);

    if (!gl_context)
        return;

    if (!gl_valid)
    {
        if (!gCoreContext->IsUIThread())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "ProcessFrame called from wrong thread");
        }
        QSize size = window.GetActualVideoDim();
        InitDisplayMeasurements(size.width(), size.height(), false);
        DestroyVideoResources();
        CreateVideoResources();
        BestDeint();
        gl_valid = true;
    }

    bool sw_frame = codec_is_std(video_codec_id) &&
                    video_codec_id != kCodec_NONE;
    bool deint_proc = m_deinterlacing && (m_deintFilter != NULL);
    OpenGLLocker ctx_lock(gl_context);

    bool pauseframe = false;
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &av_pause_frame);
        pauseframe = true;
    }

    if (sw_frame)
    {
        CropToDisplay(frame);
    }

    bool dummy = frame->dummy;
    if (filterList && sw_frame && !dummy)
        filterList->ProcessFrame(frame);

    bool safepauseframe = pauseframe && !IsBobDeint();
    if (sw_frame && deint_proc && m_deinterlaceBeforeOSD &&
       (!pauseframe || safepauseframe) && !dummy)
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    if (!window.IsEmbedding())
    {
        gl_pipchain_active = NULL;
        ShowPIPs(frame, pipPlayers);
    }

    if (sw_frame && (!pauseframe || safepauseframe) &&
        deint_proc && !m_deinterlaceBeforeOSD && !dummy)
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    if (gl_videochain && sw_frame && !dummy)
    {
        bool soft_bob = m_deinterlacing && (m_deintfiltername == "bobdeint");
        gl_videochain->UpdateInputFrame(frame, soft_bob);
    }
}

void VideoOutputOpenGL::PrepareFrame(VideoFrame *buffer, FrameScanType t,
                                     OSD *osd)
{
    if (!gl_context)
        return;

    OpenGLLocker ctx_lock(gl_context);

    if (!buffer)
    {
        buffer = vbuffers.GetScratchFrame();
        if (m_deinterlacing && !IsBobDeint())
            t = kScan_Interlaced;
    }

    gl_context_lock.lock();
    framesPlayed = buffer->frameNumber + 1;
    gl_context_lock.unlock();

    gl_context->BindFramebuffer(0);
    if (db_letterbox_colour == kLetterBoxColour_Gray25)
        gl_context->SetBackground(127, 127, 127, 255);
    else
        gl_context->SetBackground(0, 0, 0, 255);
    gl_context->ClearFramebuffer();

    // stereoscopic views
    QRect main   = gl_context->GetViewPort();
    QRect first  = main;
    QRect second = main;
    bool twopass = (m_stereo == kStereoscopicModeSideBySide) ||
                   (m_stereo == kStereoscopicModeTopAndBottom);

    if (kStereoscopicModeSideBySide == m_stereo)
    {
        first  = QRect(main.left() / 2,  main.top(),
                       main.width() / 2, main.height());
        second = first.translated(main.width() / 2, 0);
    }
    else if (kStereoscopicModeTopAndBottom == m_stereo)
    {
        first  = QRect(main.left(),  main.top() / 2,
                       main.width(), main.height() / 2);
        second = first.translated(0, main.height() / 2);
    }

    // main UI when embedded
    MythMainWindow *mwnd = GetMythMainWindow();
    if (gl_context->IsShared() && mwnd && mwnd->GetPaintWindow() &&
        window.IsEmbedding())
    {
        if (twopass)
            gl_context->SetViewPort(first, true);
        mwnd->GetPaintWindow()->setMask(QRegion());
        mwnd->draw();
        if (twopass)
        {
            gl_context->SetViewPort(second, true);
            mwnd->GetPaintWindow()->setMask(QRegion());
            mwnd->draw();
            gl_context->SetViewPort(main, true);
        }
    }

    // video
    if (gl_videochain && !buffer->dummy)
    {
        gl_videochain->SetVideoRect(vsz_enabled ? vsz_desired_display_rect :
                                                  window.GetDisplayVideoRect(),
                                    window.GetVideoRect());
        gl_videochain->PrepareFrame(buffer->top_field_first, t,
                                    m_deinterlacing, framesPlayed, m_stereo);
    }

    // PiPs/PBPs
    if (gl_pipchains.size())
    {
        QMap<MythPlayer*,OpenGLVideo*>::iterator it = gl_pipchains.begin();
        for (; it != gl_pipchains.end(); ++it)
        {
            if (gl_pip_ready[it.key()])
            {
                bool active = gl_pipchain_active == *it;
                if (twopass)
                    gl_context->SetViewPort(first, true);
                (*it)->PrepareFrame(buffer->top_field_first, t,
                                    m_deinterlacing, framesPlayed,
                                    kStereoscopicModeNone, active);
                if (twopass)
                {
                    gl_context->SetViewPort(second, true);
                    (*it)->PrepareFrame(buffer->top_field_first, t,
                                    m_deinterlacing, framesPlayed,
                                    kStereoscopicModeNone, active);
                    gl_context->SetViewPort(main);
                }
            }
        }
    }

    // visualisation
    if (m_visual && gl_painter && !window.IsEmbedding())
    {
        if (twopass)
            gl_context->SetViewPort(first, true);
        m_visual->Draw(GetTotalOSDBounds(), gl_painter, NULL);
        if (twopass)
        {
            gl_context->SetViewPort(second, true);
            m_visual->Draw(GetTotalOSDBounds(), gl_painter, NULL);
            gl_context->SetViewPort(main);
        }
    }

    // OSD
    if (osd && gl_painter && !window.IsEmbedding())
    {
        if (twopass)
            gl_context->SetViewPort(first, true);
        osd->DrawDirect(gl_painter, GetTotalOSDBounds().size(), true);
        if (twopass)
        {
            gl_context->SetViewPort(second, true);
            osd->DrawDirect(gl_painter, GetTotalOSDBounds().size(), true);
            gl_context->SetViewPort(main);
        }
    }

    gl_context->Flush(false);

    if (vbuffers.GetScratchFrame() == buffer)
        vbuffers.SetLastShownFrameToScratch();
}

void VideoOutputOpenGL::Show(FrameScanType scan)
{
    OpenGLLocker ctx_lock(gl_context);
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "IsErrored() is true in Show()");
        return;
    }

    if (gl_context)
        gl_context->swapBuffers();
}

QStringList VideoOutputOpenGL::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;

    QStringList list;

    if (codec_is_std(myth_codec_id) && !getenv("NO_OPENGL"))
    {
        list << "opengl" << "opengl-lite";
    }

    return list;
}

void VideoOutputOpenGL::Zoom(ZoomDirection direction)
{
    QMutexLocker locker(&gl_context_lock);
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputOpenGL::MoveResize(void)
{
    QMutexLocker locker(&gl_context_lock);
    VideoOutput::MoveResize();
    if (gl_videochain)
    {
        gl_videochain->SetVideoRect(vsz_enabled ? vsz_desired_display_rect :
                                                  window.GetDisplayVideoRect(),
                                    window.GetVideoRect());
    }
}

void VideoOutputOpenGL::UpdatePauseFrame(int64_t &disp_timecode)
{
    QMutexLocker locker(&gl_context_lock);
    VideoFrame *used_frame = vbuffers.Head(kVideoBuffer_used);
    if (!used_frame)
        used_frame = vbuffers.GetScratchFrame();

    CopyFrame(&av_pause_frame, used_frame);
    disp_timecode = av_pause_frame.disp_timecode;
}

void VideoOutputOpenGL::InitPictureAttributes(void)
{
    if (video_codec_id == kCodec_NONE)
        return;

    videoColourSpace.SetSupportedAttributes((PictureAttributeSupported)
                                       (kPictureAttributeSupported_Brightness |
                                        kPictureAttributeSupported_Contrast |
                                        kPictureAttributeSupported_Colour |
                                        kPictureAttributeSupported_Hue |
                                        kPictureAttributeSupported_StudioLevels));
}

int VideoOutputOpenGL::SetPictureAttribute(PictureAttribute attribute,
                                           int newValue)
{
    if (!gl_context)
        return -1;

    return VideoOutput::SetPictureAttribute(attribute, newValue);
}

bool VideoOutputOpenGL::SetupDeinterlace(
    bool interlaced, const QString &overridefilter)
{
    if (!gl_videochain || !gl_context)
        return false;

    OpenGLLocker ctx_lock(gl_context);

    if (db_vdisp_profile)
        m_deintfiltername = db_vdisp_profile->GetFilteredDeint(overridefilter);

    if (!m_deintfiltername.contains("opengl"))
    {
        gl_videochain->SetDeinterlacing(false);
        gl_videochain->SetSoftwareDeinterlacer(QString::null);
        VideoOutput::SetupDeinterlace(interlaced, overridefilter);
        if (m_deinterlacing)
            gl_videochain->SetSoftwareDeinterlacer(m_deintfiltername);

        return m_deinterlacing;
    }

    // clear any non opengl filters
    if (m_deintFiltMan)
    {
        delete m_deintFiltMan;
        m_deintFiltMan = NULL;
    }
    if (m_deintFilter)
    {
        delete m_deintFilter;
        m_deintFilter = NULL;
    }

    MoveResize();
    m_deinterlacing = interlaced;

    if (m_deinterlacing && !m_deintfiltername.isEmpty())
    {
        if (gl_videochain->GetDeinterlacer() != m_deintfiltername)
        {
            if (!gl_videochain->AddDeinterlacer(m_deintfiltername))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Couldn't load deinterlace filter %1")
                        .arg(m_deintfiltername));
                m_deinterlacing = false;
                m_deintfiltername = "";
            }
            else
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Using deinterlace method %1")
                        .arg(m_deintfiltername));
            }
        }
    }

    gl_videochain->SetDeinterlacing(m_deinterlacing);

    return m_deinterlacing;
}

bool VideoOutputOpenGL::SetDeinterlacingEnabled(bool enable)
{
    (void) enable;

    if (!gl_videochain || !gl_context)
        return false;

    OpenGLLocker ctx_lock(gl_context);

    if (enable)
    {
        if (m_deintfiltername.isEmpty())
            return SetupDeinterlace(enable);
        if (m_deintfiltername.contains("opengl"))
        {
            if (gl_videochain->GetDeinterlacer().isEmpty())
                return SetupDeinterlace(enable);
        }
        else if (!m_deintfiltername.contains("opengl"))
        {
            // make sure opengl deinterlacing is disabled
            gl_videochain->SetDeinterlacing(false);

            if (!m_deintFiltMan || !m_deintFilter)
                return VideoOutput::SetupDeinterlace(enable);
        }
    }

    MoveResize();
    gl_videochain->SetDeinterlacing(enable);

    m_deinterlacing = enable;

    return m_deinterlacing;
}

void VideoOutputOpenGL::ShowPIP(VideoFrame  *frame,
                                MythPlayer  *pipplayer,
                                PIPLocation  loc)
{
    if (!pipplayer)
        return;

    int pipw, piph;
    VideoFrame *pipimage = pipplayer->GetCurrentFrame(pipw, piph);
    const float pipVideoAspect = pipplayer->GetVideoAspect();
    const QSize pipVideoDim    = pipplayer->GetVideoBufferSize();
    const bool  pipActive      = pipplayer->IsPIPActive();
    const bool  pipVisible     = pipplayer->IsPIPVisible();
    const uint  pipVideoWidth  = pipVideoDim.width();
    const uint  pipVideoHeight = pipVideoDim.height();

    // If PiP is not initialized to values we like, silently ignore the frame.
    if ((pipVideoAspect <= 0) || !pipimage ||
        !pipimage->buf || pipimage->codec != FMT_YV12)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    if (!pipVisible)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(loc, pipplayer);
    QRect dvr = window.GetDisplayVisibleRect();

    gl_pip_ready[pipplayer] = false;
    OpenGLVideo *gl_pipchain = gl_pipchains[pipplayer];
    if (!gl_pipchain)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Initialise PiP.");
        gl_pipchains[pipplayer] = gl_pipchain = new OpenGLVideo();
        QString options = GetFilters();
        if (gl_opengl_lite)
            options += " preferycbcr";
        bool success = gl_pipchain->Init(gl_context, &videoColourSpace,
                     pipVideoDim, pipVideoDim,
                     dvr, position,
                     QRect(0, 0, pipVideoWidth, pipVideoHeight),
                     false, options, false);
        QSize viewport = window.GetDisplayVisibleRect().size();
        gl_pipchain->SetMasterViewport(viewport);
        if (!success)
        {
            pipplayer->ReleaseCurrentFrame(pipimage);
            return;
        }
    }

    QSize current = gl_pipchain->GetVideoSize();
    if ((uint)current.width()  != pipVideoWidth ||
        (uint)current.height() != pipVideoHeight)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Re-initialise PiP.");
        delete gl_pipchain;
        gl_pipchains[pipplayer] = gl_pipchain = new OpenGLVideo();
        QString options = GetFilters();
        if (gl_opengl_lite)
            options += " preferycbcr";
        bool success = gl_pipchain->Init(
            gl_context, &videoColourSpace,
            pipVideoDim, pipVideoDim, dvr, position,
            QRect(0, 0, pipVideoWidth, pipVideoHeight),
            false, options, false);

        QSize viewport = window.GetDisplayVisibleRect().size();
        gl_pipchain->SetMasterViewport(viewport);

        if (!success)
        {
            pipplayer->ReleaseCurrentFrame(pipimage);
            return;
        }

    }
    gl_pipchain->SetVideoRect(position,
                              QRect(0, 0, pipVideoWidth, pipVideoHeight));
    gl_pipchain->UpdateInputFrame(pipimage);

    gl_pip_ready[pipplayer] = true;

    if (pipActive)
        gl_pipchain_active = gl_pipchain;

    pipplayer->ReleaseCurrentFrame(pipimage);
}

void VideoOutputOpenGL::RemovePIP(MythPlayer *pipplayer)
{
    if (!gl_pipchains.contains(pipplayer))
        return;

    OpenGLLocker ctx_lock(gl_context);

    OpenGLVideo *gl_pipchain = gl_pipchains[pipplayer];
    if (gl_pipchain)
        delete gl_pipchain;
    gl_pip_ready.remove(pipplayer);
    gl_pipchains.remove(pipplayer);
}

void VideoOutputOpenGL::MoveResizeWindow(QRect new_rect)
{
    if (gl_context)
        gl_context->MoveResizeWindow(new_rect);
}

void VideoOutputOpenGL::EmbedInWidget(const QRect &rect)
{
    if (!window.IsEmbedding())
        VideoOutput::EmbedInWidget(rect);

    MoveResize();
}

void VideoOutputOpenGL::StopEmbedding(void)
{
    if (!window.IsEmbedding())
        return;

    VideoOutput::StopEmbedding();
    MoveResize();
}

bool VideoOutputOpenGL::ApproveDeintFilter(const QString& filtername) const
{
    if (filtername.contains("opengl") && !gl_opengl_lite)
        return true;

    if (filtername.contains("bobdeint"))
        return true;

    return VideoOutput::ApproveDeintFilter(filtername);
}

QStringList VideoOutputOpenGL::GetVisualiserList(void)
{
    if (gl_context)
        return VideoVisual::GetVisualiserList(gl_context->Type());
    return VideoOutput::GetVisualiserList();
}
