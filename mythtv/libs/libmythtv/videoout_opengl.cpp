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

#define LOC      QString("VidOutGL: ")

/*! \brief Generate the list of available OpenGL profiles
 *
 * \note This could be improved by eliminating unsupported profiles at run time -
 * but it is currently called statically and hence options would be fixed and unable
 * to reflect changes in UI render device.
*/
void VideoOutputOpenGL::GetRenderOptions(render_opts &opts,
                                         QStringList &cpudeints)
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
    safe << "opengl" << "opengl-lite" << "opengl-yv12" << "opengl-hquyv" << "opengl-rgba";

    // all profiles can handle all software frames
    (*opts.safe_renderers)["dummy"].append(safe);
    (*opts.safe_renderers)["nuppel"].append(safe);
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append(safe);
    if (opts.decoders->contains("vda"))
        (*opts.safe_renderers)["vda"].append(safe);
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append(safe);
    if (opts.decoders->contains("openmax"))
        (*opts.safe_renderers)["openmax"].append(safe);
    if (opts.decoders->contains("mediacodec"))
        (*opts.safe_renderers)["mediacodec"].append(safe);
    if (opts.decoders->contains("vaapi2"))
        (*opts.safe_renderers)["vaapi2"].append(safe);
    if (opts.decoders->contains("nvdec"))
        (*opts.safe_renderers)["nvdec"].append(safe);

    // OpenGL UYVY
    opts.renderers->append("opengl");
    opts.deints->insert("opengl", cpudeints + gldeints);
    (*opts.deints)["opengl"].append("bobdeint");
    (*opts.osds)["opengl"].append("opengl2");
    opts.priorities->insert("opengl", 65);

    // OpenGL HQ UYV
    opts.renderers->append("opengl-hquyv");
    opts.deints->insert("opengl-hquyv", cpudeints + gldeints);
    (*opts.deints)["opengl-hquyv"].append("bobdeint");
    (*opts.osds)["opengl-hquyv"].append("opengl2");
    opts.priorities->insert("opengl-hquyv", 60);

    // OpenGL YV12
    opts.renderers->append("opengl-yv12");
    opts.deints->insert("opengl-yv12", cpudeints + gldeints);
    (*opts.deints)["opengl-yv12"].append("bobdeint");
    (*opts.osds)["opengl-yv12"].append("opengl2");
    opts.priorities->insert("opengl-yv12", 65);

    // lite (YCbCr) profile - no colourspace control, GPU deinterlacing
    opts.renderers->append("opengl-lite");
    opts.deints->insert("opengl-lite", cpudeints);
    (*opts.deints)["opengl-lite"].append("bobdeint");
    (*opts.osds)["opengl-lite"].append("opengl2");
    opts.priorities->insert("opengl-lite", 50);

    // software fallback
    opts.renderers->append("opengl-rgba");
    opts.deints->insert("opengl-rgba", cpudeints);
    (*opts.osds)["opengl-rgba"].append("opengl2");
    opts.priorities->insert("opengl-rgba", 10);
}

VideoOutputOpenGL::VideoOutputOpenGL(const QString &profile)
    : gl_context_lock(QMutex::Recursive), gl_context(nullptr), gl_valid(true),
    gl_videochain(nullptr), gl_pipchain_active(nullptr),
    gl_parent_win(0),    gl_painter(nullptr), gl_created_painter(false),
    gl_opengl_profile(profile),
    gl_opengl_type(OpenGLVideo::StringToType(profile))
{
    memset(&av_pause_frame, 0, sizeof(av_pause_frame));
    av_pause_frame.buf = nullptr;

    if (gCoreContext->GetBoolSetting("UseVideoModes", false))
        display_res = DisplayRes::GetDisplayRes(true);
}

VideoOutputOpenGL::~VideoOutputOpenGL()
{
    gl_context_lock.lock();
    VideoOutputOpenGL::TearDown();

    if (gl_context)
        gl_context->DecrRef();
    gl_context = nullptr;
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
    if (result)
    {
        QSize size = window.GetActualVideoDim();
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
    gl_context_lock.lock();
    DiscardFrames(true);
    vbuffers.DeleteBuffers();
    vbuffers.Reset();

    if (av_pause_frame.buf)
    {
        av_freep(&av_pause_frame.buf);
    }
    if (av_pause_frame.qscale_table)
    {
        av_freep(&av_pause_frame.qscale_table);
    }
    gl_context_lock.unlock();
}

void VideoOutputOpenGL::DestroyGPUResources(void)
{
    gl_context_lock.lock();
    if (gl_context)
        gl_context->makeCurrent();

#ifdef USE_OPENGL_QT5
    if (gl_created_painter)
    {
        QWidget *device = QWidget::find(gl_parent_win);
        if (device)
            device->setAttribute(Qt::WA_PaintOnScreen, false);
    }
#endif

    if (gl_created_painter)
    {
        // Hack to ensure that the osd painter is not
        // deleted while image load thread is still busy
        // loading images with that painter
        gl_painter->Teardown();
        delete invalid_osd_painter;
        invalid_osd_painter = gl_painter;
    }
    else if (gl_painter)
        gl_painter->SetSwapControl(true);

    gl_painter = nullptr;
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
        gl_videochain = nullptr;
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
        db_vdisp_profile->SetVideoRenderer(gl_opengl_profile);
}

bool VideoOutputOpenGL::InputChanged(const QSize &video_dim_buf,
                                     const QSize &video_dim_disp,
                                     float        aspect,
                                     MythCodecID  av_codec_id,
                                     void        */*codec_private*/,
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

    if (!codec_is_std(av_codec_id)
        && !codec_is_mediacodec(av_codec_id)
        && !codec_is_vaapi2(av_codec_id)
        && !codec_is_nvdec(av_codec_id))
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

    // This code does not work.
    // Using OpenGL video renderer with QT Theme painter - the moment
    // the code below calls gl_context->create() the main window disappears
    // and after that the video renderer complains about rendering on a non
    // visible window. The window never comes back and you have to kill
    // the frontend.

/*
    QWidget *device = QWidget::find(gl_parent_win);
    if (!device)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find parent to windowID");
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
    gl_context = nullptr;
*/
    LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to use OpenGL when ThemePainter is set to QT.");
    return false;
}

bool VideoOutputOpenGL::SetupOpenGL(void)
{
    if (!gl_context)
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
    OpenGLVideo::VideoType type = codec_sw_copy(video_codec_id) ? gl_opengl_type : OpenGLVideo::kGLGPU;
    success = gl_videochain->Init(gl_context, &videoColourSpace,
                                  window.GetVideoDim(),
                                  window.GetVideoDispDim(), dvr,
                                  window.GetDisplayVideoRect(),
                                  window.GetVideoRect(), true,
                                  type, options);
    if (success)
    {
        // check if the profile changed
        if (codec_sw_copy(video_codec_id))
        {
            gl_opengl_type    = gl_videochain->GetType();
            gl_opengl_profile = OpenGLVideo::TypeToString(gl_opengl_type);
        }

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
        QWidget *device = QWidget::find(gl_parent_win);
        gl_painter = new MythOpenGLPainter(gl_context, device);
        if (!gl_painter)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create painter");
            return;
        }
        gl_created_painter = true;
#ifdef USE_OPENGL_QT5
        if (device)
            device->setAttribute(Qt::WA_PaintOnScreen);
#endif
    }
    else
    {
        gl_painter = dynamic_cast<MythOpenGLPainter*>(win->GetCurrentPainter());
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
    int size = buffersize(FMT_YV12,
                          vbuffers.GetScratchFrame()->width,
                          vbuffers.GetScratchFrame()->height);
    unsigned char *buffer = (unsigned char *)av_malloc(size);
    init(&av_pause_frame, FMT_YV12,
         buffer,
         vbuffers.GetScratchFrame()->width,
         vbuffers.GetScratchFrame()->height,
         size);

    av_pause_frame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    if (!av_pause_frame.buf)
        return false;

    clear(&av_pause_frame);
    return true;
}

void VideoOutputOpenGL::ProcessFrame(VideoFrame *frame, OSD */*osd*/,
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

    bool sw_frame = codec_sw_copy(video_codec_id) &&
                    video_codec_id != kCodec_NONE;
    bool deint_proc = m_deinterlacing && (m_deintFilter != nullptr);
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
        gl_pipchain_active = nullptr;
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

    bool dummy = false;
    gl_context_lock.lock();
    if (buffer)
    {
        framesPlayed = buffer->frameNumber + 1;
        dummy = buffer->dummy;
    }
    gl_context_lock.unlock();

    if (!buffer)
    {
        buffer = vbuffers.GetScratchFrame();
        if (m_deinterlacing && !IsBobDeint())
            t = kScan_Interlaced;
    }

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
    if (mwnd && mwnd->GetPaintWindow() && window.IsEmbedding())
    {
        if (twopass)
            gl_context->SetViewPort(first, true);
        mwnd->GetPaintWindow()->clearMask();
        // Must force a UI redraw when embedded.  If not, when the EPG or
        // finder screen is popped up over the video and the user then clicks
        // away from Myth, the UI is left blank.
        mwnd->GetMainStack()->GetTopScreen()->SetRedraw();
        mwnd->draw(gl_painter);
        if (twopass)
        {
            gl_context->SetViewPort(second, true);
            mwnd->GetPaintWindow()->clearMask();
            mwnd->GetMainStack()->GetTopScreen()->SetRedraw();
            mwnd->draw(gl_painter);
            gl_context->SetViewPort(main, true);
        }
    }

    // video
    if (gl_videochain && !dummy)
    {
        gl_videochain->SetVideoRect(vsz_enabled ? vsz_desired_display_rect :
                                                  window.GetDisplayVideoRect(),
                                    window.GetVideoRect());
        gl_videochain->PrepareFrame(buffer->top_field_first, t,
                                    m_deinterlacing, framesPlayed, m_stereo);
    }

    // PiPs/PBPs
    if (!gl_pipchains.empty())
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
        m_visual->Draw(GetTotalOSDBounds(), gl_painter, nullptr);
        if (twopass)
        {
            gl_context->SetViewPort(second, true);
            m_visual->Draw(GetTotalOSDBounds(), gl_painter, nullptr);
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
}

void VideoOutputOpenGL::Show(FrameScanType /*scan*/)
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

/*! \brief Generate a list of supported OpenGL profiles.
 *
 * \note This list could be filtered based upon current feature support. This
 * would however assume an OpenGL render device (not currently a given) but more
 * importantly, filtering out a selected profile encourages the display profile
 * code to use a higher priority, non-OpenGL renderer (such as VDPAU). By not
 * filtering, we allow the OpenGL video code to fallback to a supported, reasonable
 * alternative.
*/
QStringList VideoOutputOpenGL::GetAllowedRenderers(MythCodecID myth_codec_id, const QSize& /*video_dim*/)
{
    QStringList list;
    if (!codec_sw_copy(myth_codec_id) || getenv("NO_OPENGL"))
        return list;

    list << "opengl" << "opengl-lite" << "opengl-yv12" << "opengl-hquyv" << "opengl-rgba";
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

    if (MythCodecContext::isCodecDeinterlacer(m_deintfiltername))
        return false;

    if (!m_deintfiltername.contains("opengl"))
    {
        gl_videochain->SetDeinterlacing(false);
        gl_videochain->SetSoftwareDeinterlacer(QString());
        VideoOutput::SetupDeinterlace(interlaced, overridefilter);
        if (m_deinterlacing)
            gl_videochain->SetSoftwareDeinterlacer(m_deintfiltername);

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

void VideoOutputOpenGL::ShowPIP(VideoFrame  */*frame*/,
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
        bool success = gl_pipchain->Init(gl_context, &videoColourSpace,
                     pipVideoDim, pipVideoDim,
                     dvr, position,
                     QRect(0, 0, pipVideoWidth, pipVideoHeight),
                     false, gl_opengl_type, options);
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
        bool success = gl_pipchain->Init(
            gl_context, &videoColourSpace,
            pipVideoDim, pipVideoDim, dvr, position,
            QRect(0, 0, pipVideoWidth, pipVideoHeight),
            false, gl_opengl_type, options);

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
    // anything OpenGL when using shaders
    if (filtername.contains("opengl") &&
        ((OpenGLVideo::kGLRGBA != gl_opengl_type) && (OpenGLVideo::kGLYCbCr != gl_opengl_type) &&
         (OpenGLVideo::kGLGPU != gl_opengl_type)))
    {
        return true;
    }

    // anything software based
    if (!filtername.contains("vdpau") && !filtername.contains("vaapi") && (OpenGLVideo::kGLGPU != gl_opengl_type))
        return true;

    return VideoOutput::ApproveDeintFilter(filtername);
}

QStringList VideoOutputOpenGL::GetVisualiserList(void)
{
    if (gl_context)
        return VideoVisual::GetVisualiserList(gl_context->Type());
    return VideoOutput::GetVisualiserList();
}

//virtual
MythPainter *VideoOutputOpenGL::GetOSDPainter(void)
{
    return gl_painter;
}

// virtual
bool VideoOutputOpenGL::CanVisualise(AudioPlayer *audio, MythRender */*render*/)
{
    return VideoOutput::CanVisualise(audio, gl_context);
}

// virtual
bool VideoOutputOpenGL::SetupVisualisation(AudioPlayer *audio,
                                MythRender */*render*/, const QString &name)
{
    return VideoOutput::SetupVisualisation(audio, gl_context, name);
}
