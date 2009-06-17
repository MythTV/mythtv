#include <QApplication>
#include <QDesktopWidget>

#include "mythcontext.h"
#include "NuppelVideoPlayer.h"
#include "videooutbase.h"
#include "util-vdpau.h"
#include "videoout_vdpau.h"
#include "videodisplayprofile.h"
#include "osd.h"
#include "osdsurface.h"
#include "mythxdisplay.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"

#define LOC      QString("VidOutVDPAU: ")
#define LOC_ERR  QString("VidOutVDPAU Error: ")
#define NUM_VDPAU_BUFFERS 17

VideoOutputVDPAU::VideoOutputVDPAU(MythCodecID codec_id)
  : VideoOutput(),
    m_codec_id(codec_id), m_display_res(NULL), m_win(0),
    m_disp(NULL), m_ctx(NULL), m_colorkey(0x020202),
    m_lock(QMutex::Recursive), m_osd_avail(false), m_pip_avail(true)
{
    if (gContext->GetNumSetting("UseVideoModes", 0))
        m_display_res = DisplayRes::GetDisplayRes();
}

VideoOutputVDPAU::~VideoOutputVDPAU()
{
    QMutexLocker locker(&m_lock);
    TearDown();
    if (m_display_res)
        m_display_res->SwitchToGUI();
}

void VideoOutputVDPAU::TearDown(void)
{
    QMutexLocker locker(&m_lock);
    m_pip_avail = true;
    m_osd_avail = false;
    DeleteBuffers();
    DeleteContext();
    DeleteXDisplay();
}

bool VideoOutputVDPAU::Init(int width, int height, float aspect, WId winid,
                            int winx, int winy, int winw, int winh, WId embedid)
{
    (void) embedid;
    QMutexLocker locker(&m_lock);
    windows[0].SetNeedRepaint(true);
    windows[0].SetAllowPreviewEPG(false);
    bool ok = VideoOutput::Init(width, height, aspect,
                                winid, winx, winy, winw, winh,
                                embedid);
    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("vdpau");
    ok = ok ? InitXDisplay(winid) : ok;
    InitDisplayMeasurements(width, height);
    ok = ok ? InitContext()       : ok;
    ok = ok ? InitBuffers()       : ok;
    if (!ok)
    {
        TearDown();
        return ok;
    }

    if (db_use_picture_controls)
        InitPictureAttributes();
    MoveResize();
    VERBOSE(VB_PLAYBACK, LOC +
            QString("Created VDPAU context (%1 decode)")
            .arg((m_codec_id < kCodec_NORMAL_END) ? "software" : "GPU"));

    if (m_ctx->InitOSD(GetTotalOSDBounds().size()))
        m_osd_avail = true;
    else
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("Failed to create VDPAU osd."));

    return ok;
}

bool VideoOutputVDPAU::InitContext(void)
{
    if (m_ctx || !m_disp)
        return false;

    QMutexLocker locker(&m_lock);
    const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();
    m_ctx = new VDPAUContext();
    bool ok = m_ctx;
    ok = ok ? m_ctx->Init(m_disp, m_win,
                          display_visible_rect.size(),
                          db_use_picture_controls,
                          m_colorkey, m_codec_id): ok;
    if (!ok)
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to initialise VDPAU"));
    return ok;
}

void VideoOutputVDPAU::DeleteContext(void)
{
    QMutexLocker locker(&m_lock);
    if (m_ctx)
    {
        m_ctx->Deinit();
        delete m_ctx;
    }
    m_ctx = NULL;
}

bool VideoOutputVDPAU::InitBuffers(void)
{
    if (!m_ctx)
        return false;

    QMutexLocker locker(&m_lock);
    const QSize video_dim = windows[0].GetVideoDim();
    vbuffers.Init(NUM_VDPAU_BUFFERS, false, 1, 4, 4, 1, false);
    bool ok = m_ctx->InitBuffers(video_dim.width(), video_dim.height(),
                                 NUM_VDPAU_BUFFERS, db_letterbox_colour);

    if ((m_codec_id > kCodec_VDPAU_BEGIN) &&
        (m_codec_id < kCodec_VDPAU_END) && ok)
    {
        ok = vbuffers.CreateBuffers(video_dim.width(),
                                    video_dim.height(), m_ctx);
    }
    else if ((m_codec_id < kCodec_NORMAL_END) && ok)
    {
        ok = vbuffers.CreateBuffers(video_dim.width(), video_dim.height());
    }

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unable to create VDPAU buffers"));
        DeleteBuffers();
    }
    return ok;
}

void VideoOutputVDPAU::DeleteBuffers(void)
{
    QMutexLocker locker(&m_lock);
    DiscardFrames(true);
    if (m_ctx)
        m_ctx->FreeBuffers();
    vbuffers.Reset();
    vbuffers.DeleteBuffers();
}

bool VideoOutputVDPAU::InitXDisplay(WId wid)
{
    QMutexLocker locker(&m_lock);
    bool ok = true;
    if (wid <= 0)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Invalid Window ID."));
        ok = false;
    }

    if (ok)
    {
        m_disp = OpenMythXDisplay();
        if (!m_disp)
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Failed to open display."));
            ok = false;
        }
    }

    if (ok)
    {
        m_win = wid;
        // if the color depth is less than 24 just use black for colorkey
        int depth = m_disp->GetDepth();
        if (depth < 24)
            m_colorkey = 0x0;
        VERBOSE(VB_PLAYBACK, LOC + QString("VDPAU Colorkey: 0x%1 (depth %2)")
                .arg(m_colorkey, 0, 16).arg(depth));

        if (!m_disp->CreateGC(m_win))
        {
            VERBOSE(VB_PLAYBACK, LOC + QString("Failed to create GC."));
            ok = false;
        }
    }

    if (!ok)
        DeleteXDisplay();

    return ok;
}

void VideoOutputVDPAU::DeleteXDisplay(void)
{
    QMutexLocker locker(&m_lock);

    const QRect tmp_display_visible_rect =
        windows[0].GetTmpDisplayVisibleRect();
    if (windows[0].GetPIPState() == kPIPStandAlone &&
        !tmp_display_visible_rect.isEmpty())
    {
        windows[0].SetDisplayVisibleRect(tmp_display_visible_rect);
    }
    const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();

    if (m_disp)
    {
        m_disp->SetForeground(m_disp->GetBlack());
        m_disp->FillRectangle(m_win, display_visible_rect);
        delete m_disp;
        m_disp = NULL;
    }
}

bool VideoOutputVDPAU::SetDeinterlacingEnabled(bool interlaced)
{
    if (!m_ctx)
        return false;

    if (m_ctx->GetDeinterlacer() != m_deintfiltername)
        return SetupDeinterlace(interlaced);

    m_deinterlacing = m_ctx->SetDeinterlacing(interlaced);
    return m_deinterlacing;
}

bool VideoOutputVDPAU::SetupDeinterlace(bool interlaced,
                                        const QString &override)
{
    if (!m_ctx)
        return false;

    m_deintfiltername = db_vdisp_profile->GetFilteredDeint(override);
    if (!m_deintfiltername.contains("vdpau"))
        return false;

    m_ctx->SetDeinterlacer(m_deintfiltername);
    m_deinterlacing = m_ctx->SetDeinterlacing(interlaced);
    return m_deinterlacing;
}

bool VideoOutputVDPAU::ApproveDeintFilter(const QString &filtername) const
{
    return filtername.contains("vdpau");
}

void VideoOutputVDPAU::ProcessFrame(VideoFrame *frame, OSD *osd,
                                    FilterChain *filterList,
                                    const PIPMap &pipPlayers,
                                    FrameScanType scan)
{
    if (m_ctx->IsErrored())
        errorState = m_ctx->GetError();

    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() in ProcessFrame()");
        return;
    }

    if (m_osd_avail && osd)
        DisplayOSD(frame, osd);
    ShowPIPs(frame, pipPlayers);
}

void VideoOutputVDPAU::PrepareFrame(VideoFrame *frame, FrameScanType scan)
{
    if (m_ctx->IsErrored())
        errorState = m_ctx->GetError();

    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() in PrepareFrame()");
        return;
    }

    if (frame)
        framesPlayed = frame->frameNumber + 1;

    if (!m_ctx)
        return;

    m_ctx->PrepareVideo(
        frame, windows[0].GetVideoRect(), 
        vsz_enabled ? vsz_desired_display_rect :
                      windows[0].GetDisplayVideoRect(),
        windows[0].GetDisplayVisibleRect().size(), scan);

    if (!frame)
        vbuffers.SetLastShownFrameToScratch();
}

void VideoOutputVDPAU::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    if (m_ctx->IsErrored())
        errorState = m_ctx->GetError();

    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() is true in DrawSlice()");
        return;
    }

    if (m_ctx)
        m_ctx->Decode(frame);
}

void VideoOutputVDPAU::Show(FrameScanType scan)
{
    if (!m_ctx)
        return;
    if (m_ctx->IsErrored())
        errorState = m_ctx->GetError();
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() is true in Show()");
        return;
    }

    if (windows[0].IsRepaintNeeded() && !windows[0].IsEmbedding())
        DrawUnusedRects(false);

    m_ctx->DisplayNextFrame();
    m_disp->Sync();
    CheckFrameStates();
}

void VideoOutputVDPAU::ClearAfterSeek(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "ClearAfterSeek()");
    DiscardFrames(false);
}

bool VideoOutputVDPAU::InputChanged(const QSize &input_size,
                                    float        aspect,
                                    MythCodecID  av_codec_id,
                                    void        *codec_private)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("InputChanged(%1,%2,%3) '%4'->'%5'")
            .arg(input_size.width()).arg(input_size.height()).arg(aspect)
            .arg(toString(m_codec_id)).arg(toString(av_codec_id)));

    QMutexLocker locker(&m_lock);
    bool cid_changed = (m_codec_id != av_codec_id);
    bool res_changed = input_size  != windows[0].GetVideoDispDim();
    bool asp_changed = aspect      != windows[0].GetVideoAspect();
    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private);

    if (!res_changed && !cid_changed)
    {
        if (asp_changed)
            MoveResize();
        return true;
    }

    m_codec_id = av_codec_id;
    TearDown();
    if (Init(input_size.width(), input_size.height(),
             aspect, m_win,
             windows[0].GetVideoRect().left(),
             windows[0].GetVideoRect().top(),
             windows[0].GetDisplayVisibleRect().width(),
             windows[0].GetDisplayVisibleRect().height(),
             0))
    {
        BestDeint();
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to recreate context."));
    return false;
}

void VideoOutputVDPAU::Zoom(ZoomDirection direction)
{
    QMutexLocker locker(&m_lock);
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputVDPAU::VideoAspectRatioChanged(float aspect)
{
    QMutexLocker locker(&m_lock);
    VideoOutput::VideoAspectRatioChanged(aspect);
}

void VideoOutputVDPAU::EmbedInWidget(int x, int y,int w, int h)
{
    QMutexLocker locker(&m_lock);
    if (!windows[0].IsEmbedding())
        VideoOutput::EmbedInWidget(x, y, w, h);
    MoveResize();
}

void VideoOutputVDPAU::StopEmbedding(void)
{
    if (!windows[0].IsEmbedding())
        return;
    QMutexLocker locker(&m_lock);
    VideoOutput::StopEmbedding();
    MoveResize();
}

void VideoOutputVDPAU::ResizeForGui(void)
{
    if (m_display_res)
        m_display_res->SwitchToGUI();
}

void VideoOutputVDPAU::ResizeForVideo(void)
{
    const QSize video_disp_dim = windows[0].GetVideoDispDim();
    ResizeForVideo(video_disp_dim.width(), video_disp_dim.height());
}

void VideoOutputVDPAU::ResizeForVideo(uint width, uint height)
{
    if ((width == 1920 || width == 1440) && height == 1088)
        height = 1080;

    if (m_display_res && m_display_res->SwitchToVideo(width, height))
    {
        windows[0].SetDisplayDim(QSize(m_display_res->GetPhysicalWidth(),
                                       m_display_res->GetPhysicalHeight()));
        windows[0].SetDisplayAspect(m_display_res->GetAspectRatio());

        bool fullscreen = !gContext->GetNumSetting("GuiSizeForTV", 0);
        if (!fullscreen)
        {
            int gui_width = 0, gui_height = 0;
            gContext->GetResolutionSetting("Gui", gui_width, gui_height);
            fullscreen |= (0 == gui_width && 0 == gui_height);
        }

        if (fullscreen)
        {
            QSize sz(m_display_res->GetWidth(), m_display_res->GetHeight());
            const QRect display_visible_rect =
                    QRect(gContext->GetMainWindow()->geometry().topLeft(), sz);
            windows[0].SetDisplayVisibleRect(display_visible_rect);
            m_disp->MoveResizeWin(m_win, display_visible_rect);
        }
    }
}

void VideoOutputVDPAU::DrawUnusedRects(bool sync)
{
    if (windows[0].IsRepaintNeeded() &&
        windows[0].GetVisibility() == kVisibility_Normal)
    {
        const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();
        m_disp->SetForeground(m_colorkey);
        m_disp->FillRectangle(m_win, display_visible_rect);
        windows[0].SetNeedRepaint(false);
    }
}

void VideoOutputVDPAU::UpdatePauseFrame(void)
{
    QMutexLocker locker(&m_lock);

    VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame() " + vbuffers.GetStatus());

    vbuffers.begin_lock(kVideoBuffer_used);
    if (vbuffers.size(kVideoBuffer_used) && m_ctx)
        m_ctx->UpdatePauseFrame(vbuffers.head(kVideoBuffer_used));
    vbuffers.end_lock();
}

void VideoOutputVDPAU::InitPictureAttributes(void)
{
    if (!m_ctx)
        return;

    supported_attributes = m_ctx->GetSupportedPictureAttributes();
    VERBOSE(VB_PLAYBACK, LOC + QString("PictureAttributes: %1")
            .arg(toString(supported_attributes)));
    VideoOutput::InitPictureAttributes();
}

int VideoOutputVDPAU::SetPictureAttribute(PictureAttribute attribute,
                                          int newValue)
{
    if (!supported_attributes || !m_ctx)
        return -1;

    newValue = min(max(newValue, 0), 100);
    newValue = m_ctx->SetPictureAttribute(attribute, newValue);
    if (newValue >= 0)
        SetPictureAttributeDBValue(attribute, newValue);
    return newValue;
}

QStringList VideoOutputVDPAU::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;
    QStringList list;

    if (((myth_codec_id < kCodec_NORMAL_END) ||
         VDPAUContext::CheckCodecSupported(myth_codec_id)) &&
         !getenv("NO_VDPAU"))
    {
        list += "vdpau";
    }

    return list;
}

MythCodecID VideoOutputVDPAU::GetBestSupportedCodec(
    uint width,       uint height,
    uint stream_type, bool no_acceleration)
{
    VideoDisplayProfile vdp;
    vdp.SetInput(QSize(width, height));
    QString dec = vdp.GetDecoder();

    MythCodecID test_cid = (MythCodecID)(kCodec_MPEG1_VDPAU + (stream_type-1));
    if ((dec != "vdpau") || getenv("NO_VDPAU") || no_acceleration ||
        !VDPAUContext::CheckCodecSupported(test_cid))
        return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));

    return test_cid;
}

int VideoOutputVDPAU::GetRefreshRate(void)
{
    return MythXGetRefreshRate();
}

void VideoOutputVDPAU::SetNextFrameDisplayTimeOffset(int delayus)
{
    if (m_ctx)
        m_ctx->SetNextFrameDisplayTimeOffset(delayus);
}

void VideoOutputVDPAU::DiscardFrame(VideoFrame *frame)
{
    if (!frame || !m_ctx)
        return;

    bool displaying = m_ctx->IsBeingUsed(frame);
    if (displaying)
        vbuffers.safeEnqueue(kVideoBuffer_displayed, frame);
    else
    {
        vbuffers.DiscardFrame(frame);
    }
}

void VideoOutputVDPAU::DiscardFrames(bool next_frame_keyframe)
{
    VERBOSE(VB_PLAYBACK, LOC + "DiscardFrames("<<next_frame_keyframe<<")");
    CheckFrameStates();
    if (m_ctx)
        m_ctx->ClearReferenceFrames();
    vbuffers.DiscardFrames(next_frame_keyframe);
    VERBOSE(VB_PLAYBACK, LOC + QString("DiscardFrames() 3: %1 -- done()")
            .arg(vbuffers.GetStatus()));
}

void VideoOutputVDPAU::DoneDisplayingFrame(void)
{
    if (vbuffers.size(kVideoBuffer_used))
    {
        VideoFrame *frame = vbuffers.head(kVideoBuffer_used);
        DiscardFrame(frame);
    }
    CheckFrameStates();
}

void VideoOutputVDPAU::CheckFrameStates(void)
{
    if (!m_ctx)
        return;

    frame_queue_t::iterator it;
    it = vbuffers.begin_lock(kVideoBuffer_displayed);
    while (it != vbuffers.end(kVideoBuffer_displayed))
    {
        VideoFrame* frame = *it;
        if (!m_ctx->IsBeingUsed(frame))
        {
            if (vbuffers.contains(kVideoBuffer_decode, frame))
            {
                VERBOSE(VB_PLAYBACK, LOC + QString(
                            "Frame %1 is in use by avlib and so is "
                            "being held for later discarding.")
                            .arg(DebugString(frame, true)));
            }
            else
            {
                vbuffers.RemoveInheritence(frame);
                vbuffers.safeEnqueue(kVideoBuffer_avail, frame);
                vbuffers.end_lock();
                it = vbuffers.begin_lock(kVideoBuffer_displayed);
                continue;
            }
        }
        ++it;
    }
    vbuffers.end_lock();
}

int VideoOutputVDPAU::DisplayOSD(VideoFrame *frame, OSD *osd,
                                  int stride, int revision)
{
    if (!osd || !m_ctx)
        return -1;

    OSDSurface *surface = osd->Display();
    if (!surface)
    {
        m_ctx->DisableOSD();
        return -1;
    }

    bool changed = (-1 == revision) ?
        surface->Changed() : (surface->GetRevision()!=revision);
    if (changed)
    {
        QSize visible = GetTotalOSDBounds().size();
        void *offsets[3], *alpha[1];
        offsets[0] = surface->y;
        offsets[1] = surface->u;
        offsets[2] = surface->v;
        alpha[0] = surface->alpha;
        m_ctx->UpdateOSD(offsets, visible, alpha);
    }
    return changed;
}

void VideoOutputVDPAU::ShowPIP(VideoFrame *frame, NuppelVideoPlayer *pipplayer,
                               PIPLocation loc)
{
    (void) frame;
    if (!pipplayer || !m_ctx)
        return;

    int pipw, piph;
    VideoFrame *pipimage       = pipplayer->GetCurrentFrame(pipw, piph);
    const bool  pipActive      = pipplayer->IsPIPActive();
    const bool  pipVisible     = pipplayer->IsPIPVisible();
    const float pipVideoAspect = pipplayer->GetVideoAspect();
    const QSize pipVideoDim    = pipplayer->GetVideoBufferSize();

    if ((pipVideoAspect <= 0) || !pipimage ||
        !pipimage->buf || pipimage->codec != FMT_YV12 || !pipVisible)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    if (m_pip_avail && 
        m_ctx->InitPIPLayer(windows[0].GetDisplayVisibleRect().size()))
    {
        m_pip_avail = m_ctx->ShowPIP(pipplayer, pipimage,
                                     GetPIPRect(loc, pipplayer), pipActive);
    }
    pipplayer->ReleaseCurrentFrame(pipimage);
}

void VideoOutputVDPAU::RemovePIP(NuppelVideoPlayer *pipplayer)
{
    if (m_ctx)
        m_ctx->DeinitPIP(pipplayer);
}

void VideoOutputVDPAU::InitDisplayMeasurements(uint width, uint height)
{
    if (!m_disp)
        return;

    bool useGuiSize = gContext->GetNumSetting("GuiSizeForTV", 0);

    if (m_display_res)
    {
        // The very first Resize needs to be the maximum possible
        // desired res, because X will mask off anything outside
        // the initial dimensions
        m_disp->MoveResizeWin(m_win,
                    QRect(gContext->GetMainWindow()->geometry().x(),
                          gContext->GetMainWindow()->geometry().y(),
                          m_display_res->GetMaxWidth(),
                          m_display_res->GetMaxHeight()));
        ResizeForVideo(width, height);
    }
    else
    {
        // The very first Resize needs to be the maximum possible
        // desired res, because X will mask off anything outside
        // the initial dimensions
        QSize sz1 = m_disp->GetDisplaySize();
        QSize sz2 = qApp->desktop()->
            availableGeometry(gContext->GetMainWindow()).size();
        int max_w = max(sz1.width(),  sz2.width());
        int max_h = max(sz1.height(), sz2.height());

        // no need to resize if we are using the same window size as the GUI
        if (useGuiSize)
        {
            max_w = gContext->GetMainWindow()->geometry().width();
            max_h = gContext->GetMainWindow()->geometry().height();
        }

        m_disp->MoveResizeWin(m_win,
                        QRect(gContext->GetMainWindow()->geometry().x(),
                              gContext->GetMainWindow()->geometry().y(),
                              max_w, max_h));

        if (db_display_dim.width() > 0 && db_display_dim.height() > 0)
        {
            windows[0].SetDisplayDim(db_display_dim);
        }
        else
        {
            windows[0].SetDisplayDim(m_disp->GetDisplayDimensions());
        }
    }
    QDesktopWidget * desktop = QApplication::desktop();
    bool             usingXinerama = (GetNumberXineramaScreens() > 1);
    int              screen = desktop->primaryScreen();
    int              w, h;

    if (usingXinerama)
    {
        screen = gContext->GetNumSetting("XineramaScreen", screen);
        if (screen >= desktop->numScreens())
            screen = 0;
    }

    if (screen == -1)
    {
        w = desktop->width();
        h = desktop->height();
    }
    else
    {
        w = desktop->screenGeometry(screen).width();
        h = desktop->screenGeometry(screen).height();
    }

    // Determine window dimensions in pixels
    int window_w = windows[0].GetDisplayVisibleRect().size().width();
    int window_h = windows[0].GetDisplayVisibleRect().size().height();
    if (useGuiSize)
        gContext->GetResolutionSetting("Gui", window_w,  window_h);
    window_w = (window_w) ? window_w : w;
    window_h = (window_h) ? window_h : h;
    float pixel_aspect = ((float)w) / ((float)h);

    VERBOSE(VB_PLAYBACK, LOC + QString(
                "Pixel dimensions: Screen %1x%2, window %3x%4")
            .arg(w).arg(h).arg(window_w).arg(window_h));

    // If the dimensions are invalid, assume square pixels and 17" screen.
    // Only print warning if this isn't Xinerama, we will fix Xinerama later.
    QSize display_dim = windows[0].GetDisplayDim();
    if (((display_dim.width() <= 0) || (display_dim.height() <= 0)) &&
        !usingXinerama)
    {
        VERBOSE(VB_GENERAL, LOC + "Physical size of display unknown."
                "\n\t\t\tAssuming 17\" monitor with square pixels.");
        display_dim.setHeight(300);
        display_dim.setWidth((int) round(300 * pixel_aspect));
    }

    // If we are using Xinerama the display dimensions can not be trusted.
    // We need to use the Xinerama monitor aspect ratio from the DB to set
    // the physical screen width. This assumes the height is correct, which
    // is more or less true in the typical side-by-side monitor setup.
    if (usingXinerama)
    {
        float displayAspect = gContext->GetFloatSettingOnHost(
            "XineramaMonitorAspectRatio",
            gContext->GetHostName(), pixel_aspect);
        int displayHeight = display_dim.height();
        if (displayHeight <= 0)
            display_dim.setHeight(displayHeight = 300);
        display_dim.setWidth((int) round(displayHeight * displayAspect));
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Estimated display dimensions: %1x%2 mm  Aspect: %3")
            .arg(display_dim.width()).arg(display_dim.height())
            .arg(((float) display_dim.width()) / display_dim.height()));

    // We must now scale the display measurements to our window size.
    // If we are running fullscreen this is a no-op.
    display_dim = QSize((display_dim.width()  * window_w) / w,
                        (display_dim.height() * window_h) / h);

    // Now make these changes stick
    windows[0].SetDisplayDim(display_dim);

    // If we are using XRandR, use the aspect ratio from it, otherwise
    // calculate the display aspect ratio pretty using physical monitor size
    if (m_display_res)
    {
        windows[0].SetDisplayAspect(m_display_res->GetAspectRatio());
    }
    else
    {
        windows[0].SetDisplayAspect(
            ((float)display_dim.width()) / display_dim.height());
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Estimated window dimensions: %1x%2 mm  Aspect: %3")
            .arg(display_dim.width()).arg(display_dim.height())
            .arg(windows[0].GetDisplayAspect()));
}

