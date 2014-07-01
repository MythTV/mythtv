// -*- Mode: c++ -*-

#include <map>
#include <iostream>
#include <algorithm>
using namespace std;

#include "mythcontext.h"
#include "videoout_d3d.h"
#include "osd.h"
#include "filtermanager.h"
#include "fourcc.h"
#include "videodisplayprofile.h"
#include "mythmainwindow.h"
#include "mythplayer.h"
#include "mythavutil.h"

#include "mmsystem.h"
#include "tv.h"

#undef UNICODE

const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 10;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;

#define NUM_DXVA2_BUFS 30

#define LOC      QString("VideoOutputD3D: ")

void VideoOutputD3D::GetRenderOptions(render_opts &opts,
                                      QStringList &cpudeints)
{
    opts.renderers->append("direct3d");
    opts.deints->insert("direct3d", cpudeints);
    (*opts.osds)["direct3d"].append("direct3d");
    (*opts.safe_renderers)["dummy"].append("direct3d");
    (*opts.safe_renderers)["nuppel"].append("direct3d");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("direct3d");
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append("direct3d");
    opts.priorities->insert("direct3d", 70);

#ifdef USING_DXVA2
    if (opts.decoders->contains("dxva2"))
        (*opts.safe_renderers)["dxva2"].append("direct3d");
#endif
}

VideoOutputD3D::VideoOutputD3D(void)
  : VideoOutput(),         m_lock(QMutex::Recursive),
    m_hWnd(NULL),          m_render(NULL),
    m_video(NULL),
    m_render_valid(false), m_render_reset(false), m_pip_active(NULL),
    m_osd_painter(NULL)
{
    m_pauseFrame.buf = NULL;
#ifdef USING_DXVA2
    m_decoder = NULL;
#endif
    m_pause_surface = NULL;
}

VideoOutputD3D::~VideoOutputD3D()
{
    TearDown();
}

void VideoOutputD3D::TearDown(void)
{
    QMutexLocker locker(&m_lock);
    vbuffers.DiscardFrames(true);
    vbuffers.Reset();
    vbuffers.DeleteBuffers();
    if (m_pauseFrame.buf)
    {
        delete [] m_pauseFrame.buf;
        m_pauseFrame.buf = NULL;
    }

    delete m_osd_painter;
    m_osd_painter = NULL;

    DeleteDecoder();
    DestroyContext();
}

void VideoOutputD3D::DestroyContext(void)
{
    QMutexLocker locker(&m_lock);
    m_render_valid = false;
    m_render_reset = false;

    while (!m_pips.empty())
    {
        delete *m_pips.begin();
        m_pips.erase(m_pips.begin());
    }
    m_pip_ready.clear();

    if (m_video)
    {
        delete m_video;
        m_video = NULL;
    }

    if (m_render)
    {
        m_render->DecrRef();
        m_render = NULL;
    }
}

void VideoOutputD3D::WindowResized(const QSize &new_size)
{
    // FIXME this now requires the context to be re-created
    /*
    QMutexLocker locker(&m_lock);
    window.SetDisplayVisibleRect(QRect(QPoint(0, 0), new_size));
    window.SetDisplayAspect(
        ((float)new_size.width()) / new_size.height());

    MoveResize();
    */
}

bool VideoOutputD3D::InputChanged(const QSize &video_dim_buf,
                                  const QSize &video_dim_disp,
                                  float        aspect,
                                  MythCodecID  av_codec_id,
                                  void        *codec_private,
                                  bool        &aspect_only)
{
    QMutexLocker locker(&m_lock);

    QSize cursize = window.GetActualVideoDim();

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("InputChanged from %1: %2x%3 aspect %4 to %5: %6x%7 aspect %9")
            .arg(toString(video_codec_id)).arg(cursize.width())
            .arg(cursize.height()).arg(window.GetVideoAspect())
            .arg(toString(av_codec_id)).arg(video_dim_disp.width())
            .arg(video_dim_disp.height()).arg(aspect));


    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = video_dim_disp != cursize;
    bool asp_changed = aspect      != window.GetVideoAspect();

    if (!res_changed && !cid_changed)
    {
        if (asp_changed)
        {
            aspect_only = true;
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        return true;
    }

    TearDown();
    QRect disp = window.GetDisplayVisibleRect();
    if (Init(video_dim_buf, video_dim_disp,
             aspect, (WId)m_hWnd, disp, av_codec_id))
    {
        BestDeint();
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to re-initialise video output.");
    errorState = kError_Unknown;

    return false;
}

bool VideoOutputD3D::SetupContext()
{
    QMutexLocker locker(&m_lock);
    DestroyContext();
    QSize size = window.GetVideoDim();
    m_render = new MythRenderD3D9();
    if (!(m_render && m_render->Create(window.GetDisplayVisibleRect().size(),
                                 m_hWnd)))
        return false;

    m_video = new D3D9Image(m_render, size, true);
    if (!(m_video && m_video->IsValid()))
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        "Direct3D device successfully initialized.");
    m_render_valid = true;
    return true;
}

bool VideoOutputD3D::Init(const QSize &video_dim_buf,
                          const QSize &video_dim_disp,
                          float aspect, WId winid,
                          const QRect &win_rect,MythCodecID codec_id)
{
    MythPainter *painter = GetMythPainter();
    if (painter)
        painter->FreeResources();

    QMutexLocker locker(&m_lock);
    m_hWnd      = (HWND)winid;
    window.SetAllowPreviewEPG(true);

    VideoOutput::Init(video_dim_buf, video_dim_disp,
                      aspect, winid, win_rect, codec_id);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Init with codec: %1")
                         .arg(toString(codec_id)));
    SetProfile();

    bool success = true;
    success &= SetupContext();
    InitDisplayMeasurements(video_dim_disp.width(), video_dim_disp.height(),
                            false);

    if (codec_is_dxva2(video_codec_id))
    {
        if (!CreateDecoder())
            return false;
    }

    success &= CreateBuffers();
    success &= InitBuffers();
    success &= CreatePauseFrame();

    MoveResize();

    if (!success)
        TearDown();
    else
    {
        m_osd_painter = new MythD3D9Painter(m_render);
        if (m_osd_painter)
        {
            m_osd_painter->SetSwapControl(false);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Created D3D9 osd painter.");
        }
        else
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to create D3D9 osd painter.");
    }
    return success;
}

void VideoOutputD3D::SetProfile(void)
{
    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("direct3d");
}

bool VideoOutputD3D::CreateBuffers(void)
{
    if (codec_is_dxva2(video_codec_id))
    {
        vbuffers.Init(NUM_DXVA2_BUFS, false, 2, 1, 4, 1);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Created %1 empty DXVA2 buffers.") .arg(NUM_DXVA2_BUFS));
        return true;
    }

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);
    return true;

}

bool VideoOutputD3D::InitBuffers(void)
{
#ifdef USING_DXVA2
    if ((codec_is_dxva2(video_codec_id)) && m_decoder)
    {
        QMutexLocker locker(&m_lock);
        const QSize video_dim = window.GetVideoDim();
        bool ok = true;
        for (int i = 0; i < NUM_DXVA2_BUFS; i++)
        {
            ok &= vbuffers.CreateBuffer(video_dim.width(),
                                        video_dim.height(), i,
                                        m_decoder->GetSurface(i), FMT_DXVA2);
        }
        if (ok)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Initialised DXVA2 buffers.");
        return ok;
    }
#endif
    return vbuffers.CreateBuffers(FMT_YV12,
                                  window.GetVideoDim().width(),
                                  window.GetVideoDim().height());
}

bool VideoOutputD3D::CreatePauseFrame(void)
{
    if (codec_is_dxva2(video_codec_id))
        return true;

    init(&m_pauseFrame, FMT_YV12,
         new unsigned char[vbuffers.GetScratchFrame()->size + 128],
         vbuffers.GetScratchFrame()->width,
         vbuffers.GetScratchFrame()->height,
         vbuffers.GetScratchFrame()->size);

    m_pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;
    return true;
}

void VideoOutputD3D::PrepareFrame(VideoFrame *buffer, FrameScanType t,
                                  OSD *osd)
{
    (void)osd;
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "PrepareFrame() called while IsErrored is true.");
        return;
    }

    if (!buffer && codec_is_std(video_codec_id))
        buffer = vbuffers.GetScratchFrame();

    bool dummy = false;
    if (buffer)
    {
        dummy = buffer->dummy;
        framesPlayed = buffer->frameNumber + 1;
    }

    if (!m_render || !m_video)
        return;

    m_render_valid = m_render->Test(m_render_reset);
    if (m_render_valid)
    {
        QRect dvr = vsz_enabled ? vsz_desired_display_rect :
                                  window.GetDisplayVideoRect();
        bool ok = m_render->ClearBuffer();
        if (ok && !dummy)
            ok = m_video->UpdateVertices(dvr, window.GetVideoRect(),
                                         255, true);

        if (ok)
        {
            ok = m_render->Begin();
            if (ok)
            {
                if (!dummy)
                    m_video->Draw();
                QMap<MythPlayer*,D3D9Image*>::iterator it = m_pips.begin();
                for (; it != m_pips.end(); ++it)
                {
                    if (m_pip_ready[it.key()])
                    {
                        if (m_pip_active == *it)
                        {
                            QRect rect = (*it)->GetRect();
                            if (!rect.isNull())
                            {
                                rect.adjust(-10, -10, 10, 10);
                                m_render->DrawRect(rect, QColor(128,0,0,255), 255);
                            }
                        }
                       (*it)->Draw();
                    }
                }

                if (m_visual)
                    m_visual->Draw(GetTotalOSDBounds(), m_osd_painter, NULL);

                if (osd && m_osd_painter && !window.IsEmbedding())
                    osd->DrawDirect(m_osd_painter, GetTotalOSDBounds().size(),
                                    true);
                m_render->End();
            }
        }

    }
}

void VideoOutputD3D::Show(FrameScanType )
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Show() called while IsErrored is true.");
        return;
    }

    if (!m_render)
        return;

    m_render_valid = m_render->Test(m_render_reset);
    if (m_render_valid)
        m_render->Present(window.IsEmbedding() ? m_hEmbedWnd : NULL);
}

void VideoOutputD3D::EmbedInWidget(const QRect &rect)
{
    if (window.IsEmbedding())
        return;

    VideoOutput::EmbedInWidget(rect);
    // TODO: Initialise m_hEmbedWnd?
}

void VideoOutputD3D::StopEmbedding(void)
{
    if (!window.IsEmbedding())
        return;

    VideoOutput::StopEmbedding();
}

void VideoOutputD3D::Zoom(ZoomDirection direction)
{
    QMutexLocker locker(&m_lock);
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputD3D::UpdatePauseFrame(int64_t &disp_timecode)
{
    QMutexLocker locker(&m_lock);
    VideoFrame *used_frame = vbuffers.Head(kVideoBuffer_used);

    if (codec_is_std(video_codec_id))
    {
        if (!used_frame)
            used_frame = vbuffers.GetScratchFrame();
        CopyFrame(&m_pauseFrame, used_frame);
        disp_timecode = m_pauseFrame.disp_timecode;
    }
    else if (codec_is_dxva2(video_codec_id))
    {
        if (used_frame)
        {
            m_pause_surface = used_frame->buf;
            disp_timecode = used_frame->disp_timecode;
        }
        else
            LOG(VB_PLAYBACK, LOG_WARNING, LOC + "Failed to update pause frame");
    }
}

void VideoOutputD3D::UpdateFrame(VideoFrame *frame, D3D9Image *img)
{
    if (codec_is_dxva2(video_codec_id))
        return;

    // TODO - add a size check
    bool hardware_conv = false;
    uint pitch = 0;
    uint8_t *buf = img->GetBuffer(hardware_conv, pitch);
    if (buf && hardware_conv)
    {
        copybuffer(buf, frame, pitch);
    }
    else if (buf && !hardware_conv)
    {
        AVPicture image_out;
        avpicture_fill(&image_out, (uint8_t*)buf,
                       PIX_FMT_RGB32, frame->width, frame->height);
        image_out.linesize[0] = pitch;
        m_copyFrame.Copy(&image_out, frame,(uint8_t*)buf, PIX_FMT_RGB32);
    }
    img->ReleaseBuffer();
}

void VideoOutputD3D::ProcessFrame(VideoFrame *frame, OSD *osd,
                                  FilterChain *filterList,
                                  const PIPMap &pipPlayers,
                                  FrameScanType scan)
{
    if (!m_render || !m_video)
        return;

    QMutexLocker locker(&m_lock);
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "ProcessFrame() called while IsErrored is true.");
        return;
    }

    bool gpu = codec_is_dxva2(video_codec_id);

    if (gpu && frame && frame->codec != FMT_DXVA2)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Wrong frame format");
        return;
    }

    bool dummy = false;
    bool pauseframe = false;
    if (!frame)
    {
        if (!gpu)
        {
            frame = vbuffers.GetScratchFrame();
            CopyFrame(vbuffers.GetScratchFrame(), &m_pauseFrame);
        }
        pauseframe = true;
    }

    CropToDisplay(frame);

    if (frame)
        dummy = frame->dummy;
    bool deint_proc = m_deinterlacing && (m_deintFilter != NULL) &&
                      !dummy;

    if (filterList && !gpu && !dummy)
        filterList->ProcessFrame(frame);

    bool safepauseframe = pauseframe && !IsBobDeint() && !gpu;
    if (deint_proc && m_deinterlaceBeforeOSD &&
       (!pauseframe || safepauseframe))
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    if (!window.IsEmbedding())
        ShowPIPs(frame, pipPlayers);

    if ((!pauseframe || safepauseframe) &&
        deint_proc && !m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    // Test the device
    m_render_valid |= m_render->Test(m_render_reset);
    if (m_render_reset)
        SetupContext();

    // Update a software decoded frame
    if (m_render_valid && !gpu && !dummy)
        UpdateFrame(frame, m_video);

    // Update a GPU decoded frame
    if (m_render_valid && gpu && !dummy)
    {
        m_render_valid = m_render->Test(m_render_reset);
        if (m_render_reset)
            CreateDecoder();

        if (m_render_valid && frame)
        {
            m_render->CopyFrame(frame->buf, m_video);
        }
        else if (m_render_valid && pauseframe)
        {
            m_render->CopyFrame(m_pause_surface, m_video);
        }
    }
}

void VideoOutputD3D::ShowPIP(VideoFrame        *frame,
                             MythPlayer *pipplayer,
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

    if ((pipVideoAspect <= 0) || !pipimage ||
        !pipimage->buf || (pipimage->codec != FMT_YV12) || !pipVisible)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(loc, pipplayer);
    QRect dvr = window.GetDisplayVisibleRect();

    m_pip_ready[pipplayer] = false;
    D3D9Image *m_pip = m_pips[pipplayer];
    if (!m_pip)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Initialise PiP.");
        m_pip = new D3D9Image(m_render, QSize(pipVideoWidth, pipVideoHeight),
                              true);
        m_pips[pipplayer] = m_pip;
        if (!m_pip->IsValid())
        {
            pipplayer->ReleaseCurrentFrame(pipimage);
            return;
        }
    }

    QSize current = m_pip->GetSize();
    if ((uint)current.width()  != pipVideoWidth ||
        (uint)current.height() != pipVideoHeight)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Re-initialise PiP.");
        delete m_pip;
        m_pip = new D3D9Image(m_render, QSize(pipVideoWidth, pipVideoHeight),
                              true);
        m_pips[pipplayer] = m_pip;
        if (!m_pip->IsValid())
        {
            pipplayer->ReleaseCurrentFrame(pipimage);
            return;
        }
    }
    m_pip->UpdateVertices(position, QRect(0, 0, pipVideoWidth, pipVideoHeight),
                          255, true);
    UpdateFrame(pipimage, m_pip);
    m_pip_ready[pipplayer] = true;
    if (pipActive)
        m_pip_active = m_pip;

    pipplayer->ReleaseCurrentFrame(pipimage);
}

void VideoOutputD3D::RemovePIP(MythPlayer *pipplayer)
{
    if (!m_pips.contains(pipplayer))
        return;

    QMutexLocker locker(&m_lock);

    D3D9Image *m_pip = m_pips[pipplayer];
    if (m_pip)
        delete m_pip;
    m_pip_ready.remove(pipplayer);
    m_pips.remove(pipplayer);
}

QStringList VideoOutputD3D::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    QStringList list;
    if (codec_is_std(myth_codec_id) || (codec_is_dxva2_hw(myth_codec_id) &&
        !getenv("NO_DXVA2")))
    {
        list += "direct3d";
    }
    return list;
}

MythPainter *VideoOutputD3D::GetOSDPainter(void)
{
    return m_osd_painter;
}

bool VideoOutputD3D::ApproveDeintFilter(const QString& filtername) const
{
    if (codec_is_std(video_codec_id))
    {
        return !filtername.contains("bobdeint") &&
               !filtername.contains("opengl") &&
               !filtername.contains("vdpau");
    }

    return false;
}

MythCodecID VideoOutputD3D::GetBestSupportedCodec(
    uint width,       uint height, const QString &decoder,
    uint stream_type, bool no_acceleration,
    PixelFormat &pix_fmt)
{
#ifdef USING_DXVA2
    QSize size(width, height);
    bool use_cpu = no_acceleration;
    MythCodecID test_cid = (MythCodecID)(kCodec_MPEG1_DXVA2 + (stream_type - 1));
    use_cpu |= !codec_is_dxva2_hw(test_cid);
    pix_fmt = PIX_FMT_DXVA2_VLD;
    if ((decoder == "dxva2") && !getenv("NO_DXVA2") && !use_cpu)
        return test_cid;
#endif
    return (MythCodecID)(kCodec_MPEG1 + (stream_type - 1));
}


void* VideoOutputD3D::GetDecoderContext(unsigned char* buf, uint8_t*& id)
{
    (void)buf;
    (void)id;
#ifdef USING_DXVA2
    if (m_decoder)
        return (void*)&m_decoder->m_context;
#endif
    return NULL;
}

bool VideoOutputD3D::CreateDecoder(void)
{
#ifdef USING_DXVA2
    QMutexLocker locker(&m_lock);
    if (m_decoder)
        DeleteDecoder();
    QSize video_dim = window.GetVideoDim();
    m_decoder = new DXVA2Decoder(NUM_DXVA2_BUFS, video_codec_id,
                                 video_dim.width(), video_dim.height());
    return (m_decoder && m_decoder->Init(m_render));
#else
    return false;
#endif
}

void VideoOutputD3D::DeleteDecoder(void)
{
#ifdef USING_DXVA2
    QMutexLocker locker(&m_lock);
    delete m_decoder;
    m_decoder = NULL;
#endif
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
