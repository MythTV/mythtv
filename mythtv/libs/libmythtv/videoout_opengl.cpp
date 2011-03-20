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
#define LOC_ERR  QString("VidOutGL Error: ")

void VideoOutputOpenGL::GetRenderOptions(render_opts &opts,
                                         QStringList &cpudeints)
{
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
    (*opts.deints)["opengl"].append("opengldoublerateyadif");
    (*opts.deints)["opengl"].append("openglyadif");
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
}

VideoOutputOpenGL::VideoOutputOpenGL()
    : VideoOutput(),
    gl_context_lock(QMutex::Recursive),
    gl_context(NULL), gl_created_context(false),
    gl_videochain(NULL), gl_pipchain_active(NULL),
    gl_parent_win(0), gl_embed_win(0),
    gl_painter(NULL), gl_created_painter(false)
{
    memset(&av_pause_frame, 0, sizeof(av_pause_frame));
    av_pause_frame.buf = NULL;

    if (gCoreContext->GetNumSetting("UseVideoModes", 0))
        display_res = DisplayRes::GetDisplayRes(true);
}

VideoOutputOpenGL::~VideoOutputOpenGL()
{
    QMutexLocker locker(&gl_context_lock);
    TearDown();

    if (gl_created_context)
        delete gl_context;
    gl_context = NULL;
}

void VideoOutputOpenGL::TearDown(void)
{
    QMutexLocker locker(&gl_context_lock);

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

    if (gl_context)
        gl_context->makeCurrent();

    if (gl_videochain)
    {
        delete gl_videochain;
        gl_videochain = NULL;
    }

    if (gl_created_painter)
        delete gl_painter;
    else
        gl_painter->SetSwapControl(true);

    gl_painter = NULL;
    gl_created_painter = false;

    while (!gl_pipchains.empty())
    {
        delete *gl_pipchains.begin();
        gl_pipchains.erase(gl_pipchains.begin());
    }
    gl_pip_ready.clear();

    if (gl_context)
        gl_context->doneCurrent();
}

bool VideoOutputOpenGL::Init(int width, int height, float aspect,
                        WId winid, int winx, int winy, int winw, int winh,
                        MythCodecID codec_id, WId embedid)
{
    QMutexLocker locker(&gl_context_lock);

    bool success = true;
    // FIXME Mac OS X overlay does not work with preview
    window.SetAllowPreviewEPG(true);
    gl_parent_win = winid;
    gl_embed_win  = embedid;

    VideoOutput::Init(width, height, aspect,
                      winid, winx, winy, winw, winh,
                      codec_id, embedid);

    SetProfile();

    success &= SetupContext();
    InitDisplayMeasurements(width, height, false);
    success &= CreateBuffers();
    success &= CreatePauseFrame();
    success &= SetupOpenGL();

    InitOSD();
    InitPictureAttributes();
    MoveResize();

    if (!success)
        TearDown();

    return success;
}

void VideoOutputOpenGL::SetProfile(void)
{
    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("opengl");
}

bool VideoOutputOpenGL::InputChanged(const QSize &input_size,
                                     float        aspect,
                                     MythCodecID  av_codec_id,
                                     void        *codec_private,
                                     bool        &aspect_only)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("InputChanged(%1,%2,%3) %4->%5")
            .arg(input_size.width()).arg(input_size.height()).arg(aspect)
            .arg(toString(video_codec_id)).arg(toString(av_codec_id)));

    QMutexLocker locker(&gl_context_lock);
    if (!codec_is_std(av_codec_id))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("New video codec is not supported."));
        errorState = kError_Unknown;
        return false;
    }

    if (input_size == window.GetActualVideoDim())
    {
        aspect_only = video_codec_id == av_codec_id;
        if (window.GetVideoAspect() != aspect)
        {
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        return true;
    }

    TearDown();
    QRect disp = window.GetDisplayVisibleRect();
    if (Init(input_size.width(), input_size.height(),
             aspect, gl_parent_win, disp.left(),  disp.top(),
             disp.width(), disp.height(), av_codec_id, gl_embed_win))
    {
        BestDeint();
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR +
        QString("Failed to re-initialise video output."));
    errorState = kError_Unknown;

    return false;
}

bool VideoOutputOpenGL::SetupContext(void)
{
    QMutexLocker locker(&gl_context_lock);

    if (gl_context)
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("Re-using context"));
        return true;
    }

    MythMainWindow* win = MythMainWindow::getMainWindow();
    if (!win)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get MythMainWindow");
        return false;
    }

    gl_created_context = false;
    //gl_context = dynamic_cast<MythRenderOpenGL*>(win->GetRenderDevice());
    if (gl_context)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Using main UI render context");
        return true;
    }

    QGLFormat fmt;
    fmt.setDepth(false);

    QGLWidget *device = (QGLWidget*)QWidget::find(gl_parent_win);
    if (!device)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to cast parent to QGLWidget");
        return false;
    }

    gl_context = MythRenderOpenGL::Create(fmt, device);
    if (gl_context && gl_context->create())
    {
        gl_context->Init();
        VERBOSE(VB_GENERAL, LOC + QString("Created MythRenderOpenGL device."));
        gl_created_context = true;
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create MythRenderOpenGL device.");
    delete gl_context;
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
    success = gl_videochain->Init(gl_context, &videoColourSpace,
                                  window.GetVideoDim(),
                                  window.GetVideoDispDim(), dvr,
                                  window.GetDisplayVideoRect(),
                                  window.GetVideoRect(), true,
                                  GetFilters(), !codec_is_std(video_codec_id),
                                  db_letterbox_colour);
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

void VideoOutputOpenGL::InitOSD(void)
{
    QMutexLocker locker(&gl_context_lock);

    gl_created_painter = false;
    MythMainWindow *win = MythMainWindow::getMainWindow();
    if (gl_created_context && gl_context)
    {
        QGLWidget *device = (QGLWidget*)QWidget::find(gl_parent_win);
        gl_painter = new MythOpenGLPainter(gl_context, device);
        if (!gl_painter)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create painter");
            return;
        }
        gl_created_painter = true;
    }
    else
    {
        gl_painter = (MythOpenGLPainter*)win->GetCurrentPainter();
        if (!gl_painter)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get painter");
            return;
        }
        VERBOSE(VB_PLAYBACK, LOC + "Using main UI painter");
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
    av_pause_frame.height = vbuffers.GetScratchFrame()->height;
    av_pause_frame.width  = vbuffers.GetScratchFrame()->width;
    av_pause_frame.bpp    = vbuffers.GetScratchFrame()->bpp;
    av_pause_frame.size   = vbuffers.GetScratchFrame()->size;
    av_pause_frame.buf    = new unsigned char[av_pause_frame.size + 128];
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

    bool sw_frame = codec_is_std(video_codec_id) && video_codec_id != kCodec_NONE;
    bool deint_proc = m_deinterlacing && (m_deintFilter != NULL);
    OpenGLLocker ctx_lock(gl_context);

    bool pauseframe = false;
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &av_pause_frame);
        pauseframe = true;
    }

    if (filterList && sw_frame)
        filterList->ProcessFrame(frame);

    bool safepauseframe = pauseframe && !IsBobDeint();
    if (sw_frame && deint_proc && m_deinterlaceBeforeOSD &&
       (!pauseframe || safepauseframe))
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    if (!window.IsEmbedding())
    {
        gl_pipchain_active = NULL;
        ShowPIPs(frame, pipPlayers);
    }

    if (sw_frame && (!pauseframe || safepauseframe) &&
        deint_proc && !m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    if (gl_videochain && sw_frame)
    {
        bool soft_bob = m_deinterlacing && (m_deintfiltername == "bobdeint");
        gl_videochain->UpdateInputFrame(frame, soft_bob);
    }
}

void VideoOutputOpenGL::PrepareFrame(VideoFrame *buffer, FrameScanType t,
                                     OSD *osd)
{
    (void)osd;
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

    if (gl_videochain)
    {
        gl_videochain->SetVideoRect(vsz_enabled ? vsz_desired_display_rect :
                                                  window.GetDisplayVideoRect(),
                                    window.GetVideoRect());
        gl_videochain->PrepareFrame(buffer->top_field_first, t,
                                    m_deinterlacing, framesPlayed);
    }
    else
    {
        gl_context->BindFramebuffer(0);
        gl_context->SetBackground(0, 0, 0, 0);
        gl_context->ClearFramebuffer();
    }

    QMap<MythPlayer*,OpenGLVideo*>::iterator it = gl_pipchains.begin();
    for (; it != gl_pipchains.end(); ++it)
    {
        if (gl_pip_ready[it.key()])
        {
            bool active = gl_pipchain_active == *it;
            (*it)->PrepareFrame(buffer->top_field_first, t,
                                m_deinterlacing, framesPlayed, active);
        }
    }

    if (osd && gl_painter && !window.IsEmbedding())
        osd->DrawDirect(gl_painter, GetTotalOSDBounds().size(), true);

    gl_context->Flush(false);

    if (vbuffers.GetScratchFrame() == buffer)
        vbuffers.SetLastShownFrameToScratch();
}

void VideoOutputOpenGL::Show(FrameScanType scan)
{
    QMutexLocker locker(&gl_context_lock);
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() is true in Show()");
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
        list += "opengl";
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
}

void VideoOutputOpenGL::UpdatePauseFrame(void)
{
    QMutexLocker locker(&gl_context_lock);
    VideoFrame *used_frame = vbuffers.head(kVideoBuffer_used);
    if (!used_frame)
        used_frame = vbuffers.GetScratchFrame();

    CopyFrame(&av_pause_frame, used_frame);
}

void VideoOutputOpenGL::InitPictureAttributes(void)
{
    if (!gl_context || video_codec_id == kCodec_NONE)
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

    return videoColourSpace.SetPictureAttribute(attribute, newValue);
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

    m_deinterlacing = interlaced;

    if (m_deinterlacing && !m_deintfiltername.isEmpty())
    {
        if (gl_videochain->GetDeinterlacer() != m_deintfiltername)
        {
            if (!gl_videochain->AddDeinterlacer(m_deintfiltername))
            {
                VERBOSE(VB_IMPORTANT, LOC +
                        QString("Couldn't load deinterlace filter %1")
                        .arg(m_deintfiltername));
                m_deinterlacing = false;
                m_deintfiltername = "";
            }
            else
            {
                VERBOSE(VB_PLAYBACK, LOC +
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

    if (gl_videochain)
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
        VERBOSE(VB_PLAYBACK, LOC + "Initialise PiP.");
        gl_pipchains[pipplayer] = gl_pipchain = new OpenGLVideo();
        bool success = gl_pipchain->Init(gl_context, &videoColourSpace,
                     pipVideoDim, pipVideoDim,
                     dvr, position,
                     QRect(0, 0, pipVideoWidth, pipVideoHeight),
                     false, GetFilters(), false);
        QSize viewport = gl_videochain ? gl_videochain->GetViewPort() :
                                         window.GetDisplayVisibleRect().size();
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
        VERBOSE(VB_PLAYBACK, LOC + "Re-initialise PiP.");
        delete gl_pipchain;
        gl_pipchains[pipplayer] = gl_pipchain = new OpenGLVideo();
        bool success = gl_pipchain->Init(
            gl_context, &videoColourSpace,
            pipVideoDim, pipVideoDim, dvr, position,
            QRect(0, 0, pipVideoWidth, pipVideoHeight),
            false, GetFilters(), false);

        QSize viewport = gl_videochain ? gl_videochain->GetViewPort() :
                                         window.GetDisplayVisibleRect().size();
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

void VideoOutputOpenGL::EmbedInWidget(int x, int y, int w, int h)
{
    if (!window.IsEmbedding())
        VideoOutput::EmbedInWidget(x,y,w,h);

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
    if (filtername.contains("opengl"))
        return true;

    if (filtername.contains("bobdeint"))
        return true;

    return VideoOutput::ApproveDeintFilter(filtername);
}
