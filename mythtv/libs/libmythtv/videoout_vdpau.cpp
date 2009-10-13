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
    m_codec_id(codec_id), m_win(0),
    m_disp(NULL), m_ctx(NULL), m_colorkey(0x020202),
    m_lock(QMutex::Recursive), m_osd_avail(false), m_pip_avail(true),
    m_buffer_size(NUM_VDPAU_BUFFERS)
{
    if (gContext->GetNumSetting("UseVideoModes", 0))
        display_res = DisplayRes::GetDisplayRes(true);
}

VideoOutputVDPAU::~VideoOutputVDPAU()
{
    QMutexLocker locker(&m_lock);
    TearDown();
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
    bool ok = VideoOutput::Init(width, height, aspect,
                                winid, winx, winy, winw, winh,
                                embedid);
    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("vdpau");
    ok = ok ? InitXDisplay(winid) : ok;
    InitDisplayMeasurements(width, height, true);
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
            .arg(codec_is_std(m_codec_id) ? "software" : "GPU"));

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
                          m_colorkey, m_codec_id, GetFilters()): ok;
    if (!ok)
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to initialise VDPAU"));
    return ok;
}

void VideoOutputVDPAU::DeleteContext(void)
{
    QMutexLocker locker(&m_lock);

    if (m_disp)
        m_disp->Lock();

    if (m_ctx)
    {
        m_ctx->Deinit();
        delete m_ctx;
    }
    m_ctx = NULL;

    if (m_disp)
        m_disp->Unlock();
}

bool VideoOutputVDPAU::InitBuffers(void)
{
    if (!m_ctx)
        return false;

    QMutexLocker locker(&m_lock);
    const QSize video_dim = windows[0].GetVideoDim();

    ParseBufferSize();

    vbuffers.Init(m_buffer_size, false, 2, 1, 4, 1, false);
    bool ok = m_ctx->InitBuffers(video_dim.width(), video_dim.height(),
                                 m_buffer_size, db_letterbox_colour);

    if (codec_is_vdpau(m_codec_id) && ok)
    {
        ok = vbuffers.CreateBuffers(video_dim.width(),
                                    video_dim.height(), m_ctx);
    }
    else if (codec_is_std(m_codec_id) && ok)
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
    {
        VideoFrame *buf = GetLastShownFrame();
        if (buf)
            buf->timecode = 0;
    }
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

    if (windows[0].IsRepaintNeeded())
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

    if (!res_changed && !cid_changed)
    {
        if (asp_changed)
        {
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        return true;
    }

    m_codec_id = av_codec_id;
    TearDown();
    QRect disp = windows[0].GetDisplayVisibleRect();
    if (Init(input_size.width(), input_size.height(),
             aspect, m_win, disp.left(), disp.top(),
             disp.width(), disp.height(), 0))
    {
        BestDeint();
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR +
        QString("Failed to re-initialise video output."));
    errorState = kError_Unknown;

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
    {
        VideoOutput::EmbedInWidget(x, y, w, h);
        MoveResize();
        windows[0].SetDisplayVisibleRect(windows[0].GetTmpDisplayVisibleRect());
    }
}

void VideoOutputVDPAU::StopEmbedding(void)
{
    if (!windows[0].IsEmbedding())
        return;
    QMutexLocker locker(&m_lock);
    VideoOutput::StopEmbedding();
    MoveResize();
}

void VideoOutputVDPAU::MoveResizeWindow(QRect new_rect)
{
    if (m_disp)
        m_disp->MoveResizeWin(m_win, new_rect);
}

void VideoOutputVDPAU::DrawUnusedRects(bool sync)
{
    if (windows[0].IsRepaintNeeded())
    {
        const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();
        m_disp->SetForeground(m_colorkey);
        m_disp->FillRectangle(m_win, display_visible_rect);
        windows[0].SetNeedRepaint(false);
    }

    if (sync)
        m_disp->Sync();
}

void VideoOutputVDPAU::UpdatePauseFrame(void)
{
    QMutexLocker locker(&m_lock);

    VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame() " + vbuffers.GetStatus());

    vbuffers.begin_lock(kVideoBuffer_used);
    if (vbuffers.size(kVideoBuffer_used) && m_ctx)
        m_ctx->UpdatePauseFrame(vbuffers.head(kVideoBuffer_used));
    else
        VERBOSE(VB_PLAYBACK,LOC_ERR +
            QString("Failed to update the pause frame."));
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
    if ((codec_is_std(myth_codec_id) || codec_is_vdpau_hw(myth_codec_id)) &&
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
    bool use_cpu = no_acceleration;
    VideoDisplayProfile vdp;
    vdp.SetInput(QSize(width, height));
    QString dec = vdp.GetDecoder();

    MythCodecID test_cid = (MythCodecID)(kCodec_MPEG1_VDPAU + (stream_type-1));
    use_cpu |= !codec_is_vdpau_hw(test_cid);
    if ((dec != "vdpau") || getenv("NO_VDPAU") || use_cpu)
        return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));

    return test_cid;
}

DisplayInfo VideoOutputVDPAU::GetDisplayInfo(void)
{
    DisplayInfo ret;
    MythXDisplay *d = OpenMythXDisplay();
    if (d)
    {
        ret.rate = d->GetRefreshRate();
        ret.res  = d->GetDisplaySize();
        ret.size = d->GetDisplayDimensions();
        delete d;
    }
    return ret;
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

void VideoOutputVDPAU::DoneDisplayingFrame(VideoFrame *frame)
{
    if (vbuffers.contains(kVideoBuffer_used, frame))
        DiscardFrame(frame);
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

void VideoOutputVDPAU::ParseBufferSize(void)
{
    QStringList list = GetFilters().split(",");
    if (list.empty())
        return;

    for (QStringList::Iterator i = list.begin(); i != list.end(); ++i)
    {
        QString name = (*i).section('=', 0, 0);
        if (!name.contains("vdpaubuffersize"))
            continue;

        uint num = (*i).section('=', 1).toUInt();
        if (6 <= num && num <= 50)
        {
            m_buffer_size = num;
            VERBOSE(VB_IMPORTANT, LOC +
                        QString("VDPAU video buffer size: %1 (default %2)")
                        .arg(m_buffer_size).arg(NUM_VDPAU_BUFFERS));
        }
    }
}
