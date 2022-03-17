// -*- Mode: c++ -*-

#include <map>
#include <iostream>
#include <algorithm>

#include "libmyth/mythcontext.h"
#include "libmythui/mythmainwindow.h"

#include "fourcc.h"
#include "mmsystem.h"
#include "mythavutil.h"
#include "mythplayer.h"
#include "mythvideoprofile.h"
#include "osd.h"
#include "tv.h"
#include "videoout_d3d.h"

extern "C" {
#include "libavutil/imgutils.h"
}

#undef UNICODE

const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 10;
const int kPrebufferFramesSmall = 4;

#define LOC      QString("VideoOutputD3D: ")

void VideoOutputD3D::GetRenderOptions(RenderOptions &Options)
{
    Options.renderers->append("direct3d");
    (*Options.safe_renderers)["dummy"].append("direct3d");
    (*Options.safe_renderers)["nuppel"].append("direct3d");
    if (Options.decoders->contains("ffmpeg"))
        (*Options.safe_renderers)["ffmpeg"].append("direct3d");
    Options.priorities->insert("direct3d", 70);

#ifdef USING_DXVA2
    if (opts.decoders->contains("dxva2"))
        (*opts.safe_renderers)["dxva2"].append("direct3d");
#endif
}

VideoOutputD3D::VideoOutputD3D(void)
  : MythVideoOutput(),
{
    m_pauseFrame.m_buffer = nullptr;
}

VideoOutputD3D::~VideoOutputD3D()
{
    TearDown();
}

void VideoOutputD3D::TearDown(void)
{
    QMutexLocker locker(&m_lock);
    m_videoBuffers.DiscardFrames(true);
    m_videoBuffers.Reset();
    m_videoBuffers.DeleteBuffers();
    if (m_pauseFrame.m_buffer)
    {
        delete [] m_pauseFrame.m_buffer;
        m_pauseFrame.m_buffer = nullptr;
    }

    if (m_osdPainter)
    {
        // Hack to ensure that the osd painter is not
        // deleted while image load thread is still busy
        // loading images with that painter
        m_osdPainter->Teardown();
        if (invalid_osd_painter)
            delete invalid_osd_painter;
        invalid_osd_painter = m_osdPainter;
        m_osdPainter = nullptr;
    }

    DeleteDecoder();
    DestroyContext();
}

void VideoOutputD3D::DestroyContext(void)
{
    QMutexLocker locker(&m_lock);
    m_renderValid = false;
    m_renderReset = false;

    while (!m_pips.empty())
    {
        delete *m_pips.begin();
        m_pips.erase(m_pips.begin());
    }
    m_pipReady.clear();

    if (m_video)
    {
        delete m_video;
        m_video = nullptr;
    }

    if (m_render)
    {
        m_render->DecrRef();
        m_render = nullptr;
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
                                  float        video_aspect,
                                  MythCodecID  av_codec_id,
                                  bool        &aspect_only,
                                  MythMultiLocker*)
{
    QMutexLocker locker(&m_lock);

    QSize cursize = m_window.GetVideoDim();

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("InputChanged from %1: %2x%3 aspect %4 to %5: %6x%7 aspect %9")
            .arg(toString(m_videoCodecID)).arg(cursize.width())
            .arg(cursize.height()).arg(m_window.GetVideoAspect())
            .arg(toString(av_codec_id)).arg(video_dim_disp.width())
            .arg(video_dim_disp.height()).arg(video_aspect));


    bool cid_changed = (m_videoCodecID != av_codec_id);
    bool res_changed = video_dim_disp != cursize;
    bool asp_changed = video_aspect   != m_window.GetVideoAspect();

    if (!res_changed && !cid_changed)
    {
        if (asp_changed)
        {
            aspect_only = true;
            VideoAspectRatioChanged(video_aspect);
            MoveResize();
        }
        return true;
    }

    TearDown();
    QRect disp = m_window.GetDisplayVisibleRect();
    if (Init(video_dim_buf, video_dim_disp,
             video_aspect, (WId)m_hWnd, disp, av_codec_id))
    {
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to re-initialise video output.");
    m_errorState = kError_Unknown;

    return false;
}

bool VideoOutputD3D::SetupContext()
{
    QMutexLocker locker(&m_lock);
    DestroyContext();
    QSize size = m_window.GetVideoDim();
    m_render = new MythRenderD3D9();
    if (!(m_render && m_render->Create(m_window.GetDisplayVisibleRect().size(),
                                 m_hWnd)))
        return false;

    m_video = new D3D9Image(m_render, size, true);
    if (!(m_video && m_video->IsValid()))
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        "Direct3D device successfully initialized.");
    m_renderValid = true;
    return true;
}

bool VideoOutputD3D::Init(const QSize &video_dim_buf,
                          const QSize &video_dim_disp,
                          float video_aspect, WId winid,
                          const QRect &win_rect,MythCodecID codec_id)
{
    MythPainter *painter = GetMythPainter();
    if (painter)
        painter->FreeResources();

    QMutexLocker locker(&m_lock);
    m_hWnd      = (HWND)winid;

    MythVideoOutput::Init(video_dim_buf, video_dim_disp,
                      video_aspect, winid, win_rect, codec_id);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Init with codec: %1")
                         .arg(toString(codec_id)));
    SetProfile();

    bool success = true;
    success &= SetupContext();
    InitDisplayMeasurements(video_dim_disp.width(), video_dim_disp.height(),
                            false);

    if (codec_is_dxva2(m_videoCodecID))
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
        m_osdPainter = new MythD3D9Painter(m_render);
        if (m_osdPainter)
        {
            m_osdPainter->SetSwapControl(false);
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
    if (m_videoProfile)
        m_videoProfile->SetVideoRenderer("direct3d");
}

bool VideoOutputD3D::CreateBuffers(void)
{
    if (codec_is_dxva2(m_videoCodecID))
    {
        m_videoBuffers.Init(VideoBuffers::GetNumBuffers(FMT_DXVA2), false, 2, 1, 4);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created %1 empty DXVA2 buffers.")
            .arg(VideoBuffers::GetNumBuffers(FMT_DXVA2)));
        return true;
    }

    m_videoBuffers.Init(VideoBuffers::GetNumBuffers(FMT_YV12), true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall);
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
    return m_videoBuffers.CreateBuffers(FMT_YV12,
                                  m_window.GetVideoDim().width(),
                                  m_window.GetVideoDim().height());
}

bool VideoOutputD3D::CreatePauseFrame(void)
{
    if (codec_is_dxva2(m_videoCodecID))
        return true;

    init(&m_pauseFrame, FMT_YV12,
         new unsigned char[m_videoBuffers.GetScratchFrame()->m_bufferSize + 128],
         m_videoBuffers.GetScratchFrame()->m_width,
         m_videoBuffers.GetScratchFrame()->m_height,
         m_videoBuffers.GetScratchFrame()->m_bufferSize);

    m_pauseFrame.m_frameNumber = m_videoBuffers.GetScratchFrame()->m_frameNumber;
    return true;
}

void VideoOutputD3D::RenderFrame(MythVideoFrame *buffer, FrameScanType t, OSD *osd)
{
    (void)osd;
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "RenderFrame() called while IsErrored is true.");
        return;
    }

    if (!buffer && codec_is_std(m_videoCodecID))
        buffer = m_videoBuffers.GetScratchFrame();

    bool dummy = false;
    if (buffer)
    {
        dummy = buffer->m_dummy;
        m_framesPlayed = buffer->m_frameNumber + 1;
    }

    if (!m_render || !m_video)
        return;

    m_renderValid = m_render->Test(m_renderReset);
    if (m_renderValid)
    {
        QRect dvr = m_window.GetITVResizing() ? m_window.GetITVDisplayRect() :
                                  m_window.GetDisplayVideoRect();
        bool ok = m_render->ClearBuffer();
        if (ok && !dummy)
            ok = m_video->UpdateVertices(dvr, m_window.GetVideoRect(),
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
                    if (m_pipReady[it.key()])
                    {
                        if (m_pipActive == *it)
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
                    m_visual->Draw(GetTotalOSDBounds(), m_osdPainter, nullptr);

                if (osd && m_osdPainter && !m_window.IsEmbedding())
                    osd->Draw(m_osdPainter, GetTotalOSDBounds().size(),
                                    true);
                m_render->End();
            }
        }

    }
}

void VideoOutputD3D::EndFrame()
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Show() called while IsErrored is true.");
        return;
    }

    if (!m_render)
        return;

    m_renderValid = m_render->Test(m_renderReset);
    if (m_renderValid)
        m_render->Present(m_window.IsEmbedding() ? m_hEmbedWnd : nullptr);
}

void VideoOutputD3D::EmbedInWidget(const QRect &rect)
{
    if (m_window.IsEmbedding())
        return;

    MythVideoOutput::EmbedInWidget(rect);
    // TODO: Initialise m_hEmbedWnd?
}

void VideoOutputD3D::StopEmbedding(void)
{
    if (!m_window.IsEmbedding())
        return;

    MythVideoOutput::StopEmbedding();
}

void VideoOutputD3D::UpdatePauseFrame(std::chrono::milliseconds &disp_timecode, FrameScanType)
{
    QMutexLocker locker(&m_lock);
    MythVideoFrame *used_frame = m_videoBuffers.Head(kVideoBuffer_used);

    if (codec_is_std(m_videoCodecID))
    {
        if (!used_frame)
            used_frame = m_videoBuffers.GetScratchFrame();
        CopyFrame(&m_pauseFrame, used_frame);
        disp_timecode = m_pauseFrame.m_displayTimecode;
    }
    else if (codec_is_dxva2(m_videoCodecID))
    {
        if (used_frame)
        {
            m_pauseSurface = used_frame->m_buffer;
            disp_timecode = used_frame->m_displayTimecode;
        }
        else
            LOG(VB_PLAYBACK, LOG_WARNING, LOC + "Failed to update pause frame");
    }
}

void VideoOutputD3D::UpdateFrame(MythVideoFrame *frame, D3D9Image *img)
{
    if (codec_is_dxva2(m_videoCodecID))
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
        AVFrame image_out;
        av_image_fill_arrays(image_out.data, image_out.linesize,
            (uint8_t*)buf,
            AV_PIX_FMT_RGB32, frame->m_width, frame->m_height, IMAGE_ALIGN);
        image_out.linesize[0] = pitch;
        m_copyFrame.Copy(&image_out, frame,(uint8_t*)buf, AV_PIX_FMT_RGB32);
    }
    img->ReleaseBuffer();
}

void VideoOutputD3D::PrepareFrame(MythVideoFrame *frame, const PIPMap &pipPlayers,
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

    bool gpu = codec_is_dxva2(m_videoCodecID);

    if (gpu && frame && frame->m_type != FMT_DXVA2)
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
            frame = m_videoBuffers.GetScratchFrame();
            CopyFrame(m_videoBuffers.GetScratchFrame(), &m_pauseFrame);
        }
        pauseframe = true;
    }

    if (frame)
        dummy = frame->m_dummy;

    if (!m_window.IsEmbedding())
        ShowPIPs(pipPlayers);

    // Test the device
    m_renderValid |= m_render->Test(m_renderReset);
    if (m_renderReset)
        SetupContext();

    // Update a software decoded frame
    if (m_renderValid && !gpu && !dummy)
        UpdateFrame(frame, m_video);

    // Update a GPU decoded frame
    if (m_renderValid && gpu && !dummy)
    {
        m_renderValid = m_render->Test(m_renderReset);
        if (m_renderReset)
            CreateDecoder();

        if (m_renderValid && frame)
        {
            m_render->CopyFrame(frame->m_buffer, m_video);
        }
        else if (m_renderValid && pauseframe)
        {
            m_render->CopyFrame(m_pauseSurface, m_video);
        }
    }
}

void VideoOutputD3D::ShowPIP(MythPlayer *pipplayer, PIPLocation loc)
{
    if (!pipplayer)
        return;

    int pipw, piph;
    MythVideoFrame *pipimage = pipplayer->GetCurrentFrame(pipw, piph);
    const float pipVideoAspect = pipplayer->GetVideoAspect();
    const QSize pipVideoDim    = pipplayer->GetVideoBufferSize();
    const bool  pipActive      = pipplayer->IsPIPActive();
    const bool  pipVisible     = pipplayer->IsPIPVisible();
    const uint  pipVideoWidth  = pipVideoDim.width();
    const uint  pipVideoHeight = pipVideoDim.height();

    if ((pipVideoAspect <= 0) || !pipimage ||
        !pipimage->m_buffer || (pipimage->m_type != FMT_YV12) || !pipVisible)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(loc, pipplayer);

    m_pipReady[pipplayer] = false;
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
    m_pipReady[pipplayer] = true;
    if (pipActive)
        m_pipActive = m_pip;

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
    m_pipReady.remove(pipplayer);
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
    return m_osdPainter;
}

MythCodecID VideoOutputD3D::GetBestSupportedCodec(
    uint width,       uint height, const QString &decoder,
    uint stream_type, bool no_acceleration,
    AVPixelFormat &pix_fmt)
{
#ifdef USING_DXVA2
    QSize size(width, height);
    bool use_cpu = no_acceleration;
    MythCodecID test_cid = (MythCodecID)(kCodec_MPEG1_DXVA2 + (stream_type - 1));
    use_cpu |= !codec_is_dxva2_hw(test_cid);
    pix_fmt = AV_PIX_FMT_DXVA2_VLD;
    if ((decoder == "dxva2") && !getenv("NO_DXVA2") && !use_cpu)
        return test_cid;
#endif
    return (MythCodecID)(kCodec_MPEG1 + (stream_type - 1));
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
    m_decoder = nullptr;
#endif
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
