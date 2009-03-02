#include <QApplication>
#include <QDesktopWidget>

#include "mythcontext.h"
#include "NuppelVideoPlayer.h"
#include "videooutbase.h"
#include "videoout_opengl.h"
#include "videodisplayprofile.h"
#include "filtermanager.h"
#include "osd.h"
#include "osdsurface.h"
#include "NuppelVideoPlayer.h"
#include "mythuihelper.h"

#define LOC      QString("VidOutOGL: ")
#define LOC_ERR  QString("VidOutOGL: ")

VideoOutputOpenGL::VideoOutputOpenGL(void)
    : VideoOutput(),
    gl_context_lock(QMutex::Recursive),
    gl_context(NULL), gl_videochain(NULL),
    gl_osdchain(NULL), gl_pipchain_active(NULL),
    gl_osd(false), gl_osd_ready(false),
    gl_parent_win(0), gl_embed_win(0), display_res(NULL)
{
    bzero(&av_pause_frame, sizeof(av_pause_frame));
    av_pause_frame.buf = NULL;

    if (gContext->GetNumSetting("UseVideoModes", 0))
        display_res = DisplayRes::GetDisplayRes();
}

VideoOutputOpenGL::~VideoOutputOpenGL()
{
    QMutexLocker locker(&gl_context_lock);
    TearDown();

    if (gl_context)
    {
        delete gl_context;
        gl_context = NULL;
    }

    if (display_res)
        display_res->SwitchToGUI();
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
        gl_context->MakeCurrent(true);

    if (gl_videochain)
    {
        delete gl_videochain;
        gl_videochain = NULL;
    }

    if (gl_osdchain)
    {
        delete gl_osdchain;
        gl_osdchain = NULL;
    }
    gl_osd = false;
    gl_osd_ready = false;

    while (!gl_pipchains.empty())
    {
        delete *gl_pipchains.begin();
        gl_pipchains.erase(gl_pipchains.begin());
    }
    gl_pip_ready.clear();

    if (gl_context)
        gl_context->MakeCurrent(false);
}

bool VideoOutputOpenGL::Init(int width, int height, float aspect,
                        WId winid, int winx, int winy, int winw, int winh,
                        WId embedid)
{
    QMutexLocker locker(&gl_context_lock);

    bool success = true;
    // FIXME Mac OS X overlay does not work with preview
    windows[0].SetAllowPreviewEPG(true);
    gl_parent_win = winid;
    gl_embed_win  = embedid;

    VideoOutput::Init(width, height, aspect,
                      winid, winx, winy, winw, winh,
                      embedid);

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("opengl");

    success &= SetupContext();
    InitDisplayMeasurements();
    success &= CreateBuffers();
    success &= SetupOpenGL();

    InitOSD();

    if (db_use_picture_controls)
        InitPictureAttributes();

    MoveResize();

    if (!success)
        TearDown();

    return success;
}

bool VideoOutputOpenGL::InputChanged(const QSize &input_size,
                                     float        aspect,
                                     MythCodecID  av_codec_id,
                                     void        *codec_private)
{
    QMutexLocker locker(&gl_context_lock);

    if (av_codec_id >= kCodec_NORMAL_END)
        return false;

    bool size_changed = (input_size != windows[0].GetVideoDim());
    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private);

    if (!size_changed)
    {
        if (windows[0].GetVideoAspect() != aspect)
            MoveResize();
        return true;
    }

    TearDown();

    bool ok = Init(input_size.width(), input_size.height(),
                   aspect, gl_parent_win,
                   windows[0].GetVideoRect().left(),
                   windows[0].GetVideoRect().top(),
                   windows[0].GetDisplayVisibleRect().width(),
                   windows[0].GetDisplayVisibleRect().height(),
                   gl_embed_win);

    if (ok)
        BestDeint();

    return ok;
}

bool VideoOutputOpenGL::SetupContext(void)
{
    QMutexLocker locker(&gl_context_lock);

    bool success = false;

    if (gl_context)
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("Re-using context"));
        success = true;
    }
    else
    {
        gl_context = OpenGLContext::Create(&gl_context_lock);
        success = gl_context->Create(gl_parent_win,
                                     windows[0].GetDisplayVisibleRect(),
                                     db_use_picture_controls);
    }
    return success;
}

bool VideoOutputOpenGL::SetupOpenGL(void)
{
    if (!gl_context)
        return false;

    const QRect dvr = windows[0].GetDisplayVisibleRect();
    if (windows[0].GetPIPState() >= kPIPStandAlone)
    {
        QRect tmprect = QRect(QPoint(0,0), dvr.size());
        ResizeDisplayWindow(tmprect, true);
    }
    bool success = false;
    OpenGLContextLocker ctx_lock(gl_context);
    gl_videochain = new OpenGLVideo();
    success = gl_videochain->Init(gl_context, db_use_picture_controls,
                                  windows[0].GetVideoDim(), dvr,
                                  windows[0].GetDisplayVideoRect(),
                                  windows[0].GetVideoRect(), true,
                                  GetFilters(), false, db_letterbox_colour);
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
    if (!db_vdisp_profile || !gl_videochain)
        return;
    if (db_vdisp_profile->GetOSDRenderer() != "opengl2")
        return;

    gl_osd = true;
    gl_osd_ready = false;

    gl_osdchain = new OpenGLVideo();
    if (!gl_osdchain->Init(
            gl_context, db_use_picture_controls,
            GetTotalOSDBounds().size(),
            GetTotalOSDBounds(), windows[0].GetDisplayVisibleRect(), 
            QRect(QPoint(0, 0), GetTotalOSDBounds().size()), false,
            GetFilters(), true))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + 
                "InitOSD(): Failed to create OpenGL2 OSD");
        delete gl_osdchain;
        gl_osdchain = NULL;
        gl_osd = false;
    }
    else
    {
        gl_osdchain->SetMasterViewport(gl_videochain->GetViewPort());
    }
}    
bool VideoOutputOpenGL::CreateBuffers(void)
{
    QMutexLocker locker(&gl_context_lock);

    bool success = true;
    vbuffers.Init(31, true, 1, 12, 4, 2, false);
    success &= vbuffers.CreateBuffers(windows[0].GetVideoDim().width(),
                                      windows[0].GetVideoDim().height());

    av_pause_frame.height = vbuffers.GetScratchFrame()->height;
    av_pause_frame.width  = vbuffers.GetScratchFrame()->width;
    av_pause_frame.bpp    = vbuffers.GetScratchFrame()->bpp;
    av_pause_frame.size   = vbuffers.GetScratchFrame()->size;
    av_pause_frame.buf    = new unsigned char[av_pause_frame.size + 128];
    av_pause_frame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    if (!av_pause_frame.buf)
        success = false;

    return success;
}

void VideoOutputOpenGL::ProcessFrame(VideoFrame *frame, OSD *osd,
                                              FilterChain *filterList,
                                              const PIPMap &pipPlayers)
{
    QMutexLocker locker(&gl_context_lock);
    if (!gl_videochain || !gl_context)
        return;

    bool deint_proc = m_deinterlacing && (m_deintFilter != NULL);
    OpenGLContextLocker ctx_lock(gl_context);

    bool pauseframe = false;
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &av_pause_frame);
        pauseframe = true;
    }

    if (filterList)
        filterList->ProcessFrame(frame);

    bool safepauseframe = pauseframe && !IsBobDeint();
    if (deint_proc && m_deinterlaceBeforeOSD &&
       (!pauseframe || safepauseframe))
    {
        m_deintFilter->ProcessFrame(frame);
    }

    if (!windows[0].IsEmbedding())
    {
        gl_pipchain_active = NULL;
        ShowPIPs(frame, pipPlayers);
        if (osd)
            DisplayOSD(frame, osd);
    }

    if ((!pauseframe || safepauseframe) &&
        deint_proc && !m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame);
    }

    bool soft_bob = m_deinterlacing && (m_deintfiltername == "bobdeint");

    if (gl_videochain)
        gl_videochain->UpdateInputFrame(frame, soft_bob);
}

void VideoOutputOpenGL::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    if (!gl_videochain || !gl_context)
        return;

    OpenGLContextLocker ctx_lock(gl_context);

    if (!buffer)
        buffer = vbuffers.GetScratchFrame();

    gl_context_lock.lock();
    framesPlayed = buffer->frameNumber + 1;
    gl_context_lock.unlock();

    if (buffer->codec != FMT_YV12)
        return;

    gl_videochain->SetVideoRect(windows[0].GetDisplayVideoRect(),
                                windows[0].GetVideoRect());
    gl_videochain->PrepareFrame(buffer->top_field_first, t,
                                m_deinterlacing, framesPlayed);

    QMap<NuppelVideoPlayer*,OpenGLVideo*>::iterator it = gl_pipchains.begin();
    for (; it != gl_pipchains.end(); ++it)
    {
        if (gl_pip_ready[it.key()])
        {
            bool active = gl_pipchain_active == *it;
            (*it)->PrepareFrame(buffer->top_field_first, t,
                                m_deinterlacing, framesPlayed, active);
        }
    }

    if (gl_osd_ready && gl_osdchain && gl_osd)
        gl_osdchain->PrepareFrame(buffer->top_field_first, t,
                                  m_deinterlacing, framesPlayed);

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
        gl_context->SwapBuffers();
}

QStringList VideoOutputOpenGL::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;

    QStringList list;

    if (myth_codec_id < kCodec_NORMAL_END &&
        !getenv("NO_OPENGL"))
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

int VideoOutputOpenGL::GetRefreshRate(void)
{
    if (gl_context)
        return gl_context->GetRefreshRate();

    return -1;
}

void VideoOutputOpenGL::InitPictureAttributes(void)
{
    if (!gl_context)
        return;

    supported_attributes = gl_context->GetSupportedPictureAttributes();

    VERBOSE(VB_PLAYBACK, LOC + QString("PictureAttributes: %1")
            .arg(toString(supported_attributes)));

    VideoOutput::InitPictureAttributes();
}

int VideoOutputOpenGL::SetPictureAttribute(
    PictureAttribute attribute, int newValue)
{
    if (!supported_attributes || !gl_context)
        return -1;

    newValue = min(max(newValue, 0), 100);
    newValue = gl_context->SetPictureAttribute(attribute, newValue);
    if (newValue >= 0)
        SetPictureAttributeDBValue(attribute, newValue);
    return newValue;
}

bool VideoOutputOpenGL::SetupDeinterlace(
    bool interlaced, const QString &overridefilter)
{
    if (!gl_videochain || !gl_context)
        return false;

    OpenGLContextLocker ctx_lock(gl_context);

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

    OpenGLContextLocker ctx_lock(gl_context);

    if (enable)
    {
        if (m_deintfiltername == "")
            return SetupDeinterlace(enable);
        if (m_deintfiltername.contains("opengl"))
        {
            if (gl_videochain->GetDeinterlacer() == "")
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

int VideoOutputOpenGL::DisplayOSD(VideoFrame *frame, OSD *osd,
                                  int stride, int revision)
{
    if (!gl_osd)
        return VideoOutput::DisplayOSD(frame, osd, stride, revision);

    gl_osd_ready = false;

    if (!osd || !gl_osdchain)
        return -1;

    if (vsz_enabled && gl_videochain)
        gl_videochain->SetVideoResize(vsz_desired_display_rect);

    OSDSurface *surface = osd->Display();
    if (!surface)
        return -1;

    gl_osd_ready = true;

    bool changed = (-1 == revision) ?
        surface->Changed() : (surface->GetRevision()!=revision);

    if (changed)
    {
        QSize visible = GetTotalOSDBounds().size();

        int offsets[3] =
        {
            surface->y - surface->yuvbuffer,
            surface->u - surface->yuvbuffer,
            surface->v - surface->yuvbuffer,
        };
        gl_osdchain->UpdateInput(surface->yuvbuffer, offsets,
                                 FMT_YV12, visible, surface->alpha);
    }
    return changed;
}

void VideoOutputOpenGL::ShowPIP(VideoFrame        *frame,
                                NuppelVideoPlayer *pipplayer,
                                PIPLocation        loc)
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
    QRect dvr = windows[0].GetDisplayVisibleRect();

    gl_pip_ready[pipplayer] = false;
    OpenGLVideo *gl_pipchain = gl_pipchains[pipplayer];
    if (!gl_pipchain)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Initialise PiP.");
        gl_pipchains[pipplayer] = gl_pipchain = new OpenGLVideo();
        bool success = gl_pipchain->Init(gl_context, db_use_picture_controls,
                     QSize(pipVideoWidth, pipVideoHeight),
                     dvr, position,
                     QRect(0, 0, pipVideoWidth, pipVideoHeight),
                     false, GetFilters());
        gl_pipchain->SetMasterViewport(gl_videochain->GetViewPort());
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
            gl_context, db_use_picture_controls,
            QSize(pipVideoWidth, pipVideoHeight),
            dvr, position,
            QRect(0, 0, pipVideoWidth, pipVideoHeight),
            false, GetFilters());

        gl_pipchain->SetMasterViewport(gl_videochain->GetViewPort());
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

void VideoOutputOpenGL::RemovePIP(NuppelVideoPlayer *pipplayer)
{
    if (!gl_pipchains.contains(pipplayer))
        return;

    OpenGLContextLocker ctx_lock(gl_context);

    OpenGLVideo *gl_pipchain = gl_pipchains[pipplayer];
    if (gl_pipchain)
        delete gl_pipchain;
    gl_pip_ready.remove(pipplayer);
    gl_pipchains.remove(pipplayer);
}

void VideoOutputOpenGL::InitDisplayMeasurements(void)
{
    if (!gl_context)
        return;

    QString source = "Actual";
    QSize screen_size;
    QRect window_rect;

    QSize disp_dim;
    gl_context->GetDisplayDimensions(disp_dim);
    windows[0].SetDisplayDim(disp_dim);

    gl_context->GetDisplaySize(screen_size);
    gl_context->GetWindowRect(window_rect);

    if (display_res)
        ResizeForVideo();

    if (db_display_dim.width() > 0 && db_display_dim.height() > 0)
    {
        windows[0].SetDisplayDim(db_display_dim);
        source = "Database";
    }

    gl_context->GetDisplaySize(screen_size);
    gl_context->GetWindowRect(window_rect);

    int xbase, ybase;
    int w = screen_size.width();
    int h = screen_size.height();
    int window_w = window_rect.width();
    int window_h = window_rect.height();

    if (w < 1 || h < 1)
    {
        GetMythUI()->GetScreenBounds(xbase, ybase, w, h);
        if (w < 1 || h < 1)
        {
            w = 1024;
            h = 768;
        }
    }

    if (gContext->GetNumSetting("GuiSizeForTV", 0))
        gContext->GetResolutionSetting("Gui", window_w,  window_h);

    window_w = (window_w > 1) ? window_w : w;
    window_h = (window_h > 1) ? window_h : h;
    float pixel_aspect = ((float)w) / ((float)h);

    VERBOSE(VB_PLAYBACK, LOC + QString(
            "Pixel dimensions: Screen %1x%2, window %3x%4")
            .arg(w).arg(h).arg(window_w).arg(window_h));

    if (gl_context->OverrideDisplayDim(disp_dim, pixel_aspect))
    {
        source = "Overridden";
        windows[0].SetDisplayDim(disp_dim);
    }

    if (windows[0].GetDisplayDim().width() < 1 ||
        windows[0].GetDisplayDim().height() < 1)
    {
        windows[0].SetDisplayDim(QSize(300, (int)((300.0 * pixel_aspect) + 0.5)));
        source = "Guessed";
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("%1 display dimensions: %2x%3mm : Aspect %4")
           .arg(source)
           .arg(windows[0].GetDisplayDim().width())
           .arg(windows[0].GetDisplayDim().height())
           .arg(((float) windows[0].GetDisplayDim().width()) /
                         windows[0].GetDisplayDim().height()));

    windows[0].SetDisplayDim(
            QSize((windows[0].GetDisplayDim().width()  * window_w) / w,
                  (windows[0].GetDisplayDim().height() * window_h) / h));

    windows[0].SetDisplayAspect(
            ((float)windows[0].GetDisplayDim().width()) /
                    windows[0].GetDisplayDim().height());

    if (display_res)
        windows[0].SetDisplayAspect(display_res->GetAspectRatio());

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Estimated window dimensions: %1x%2 mm  Aspect: %3")
            .arg(windows[0].GetDisplayDim().width())
            .arg(windows[0].GetDisplayDim().height())
            .arg(windows[0].GetDisplayAspect()));
}


void VideoOutputOpenGL::ResizeForGui(void)
{
    if (display_res)
        display_res->SwitchToGUI();
}

void VideoOutputOpenGL::ResizeForVideo(void)
{
    uint width  = windows[0].GetVideoDispDim().width();
    uint height = windows[0].GetVideoDispDim().height();

    if ((width == 1920 || width == 1440) && height == 1088)
        height = 1080;

    if (display_res && display_res->SwitchToVideo(width, height))
    {
        windows[0].SetDisplayDim(QSize(display_res->GetPhysicalWidth(),
                            display_res->GetPhysicalHeight()));
        windows[0].SetDisplayAspect(display_res->GetAspectRatio());

        bool fullscreen = !gContext->GetNumSetting("GuiSizeForTV", 0);

        // if width && height are zero users expect fullscreen playback
        if (!fullscreen)
        {
            int gui_width = 0, gui_height = 0;
            gContext->GetResolutionSetting("Gui", gui_width, gui_height);
            fullscreen |= (0 == gui_width && 0 == gui_height);
        }

        if (fullscreen)
        {
            QSize sz(display_res->GetWidth(), display_res->GetHeight());
            windows[0].SetDisplayVisibleRect(QRect(QPoint(0,0), sz));
            if (gl_context)
                gl_context->MoveResizeWindow(windows[0].GetDisplayVisibleRect());
        }
    }
}

void VideoOutputOpenGL::EmbedInWidget(int x, int y, int w, int h)
{
    if (!windows[0].IsEmbedding())
    {
        VideoOutput::EmbedInWidget(x,y,w,h);
        if (gl_context)
            gl_context->EmbedInWidget(x,y,w,h);
    }
    MoveResize();
}

void VideoOutputOpenGL::StopEmbedding(void)
{
    if (!windows[0].IsEmbedding())
        return;

    if (gl_context)
        gl_context->StopEmbedding();
    VideoOutput::StopEmbedding();
    MoveResize();
}

void VideoOutputOpenGL::ShutdownVideoResize(void)
{
    if (!gl_osdchain)
    {
        VideoOutput::ShutdownVideoResize();
        return;
    }

    if (gl_videochain)
        gl_videochain->DisableVideoResize();

    vsz_enabled = false;
}

bool VideoOutputOpenGL::ApproveDeintFilter(const QString& filtername) const
{
    if (filtername.contains("opengl"))
        return true;

    if (filtername.contains("bobdeint"))
        return true;

    return VideoOutput::ApproveDeintFilter(filtername);
}
