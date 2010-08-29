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
#include "myth_imgconvert.h"
#include "mythplayer.h"

#include "mmsystem.h"
#include "tv.h"

#undef UNICODE

extern "C" {
#include "libavcodec/avcodec.h"
}

const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 10;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;

#define LOC      QString("VideoOutputD3D: ")
#define LOC_WARN QString("VideoOutputD3D Warning: ")
#define LOC_ERR  QString("VideoOutputD3D Error: ")

void VideoOutputD3D::GetRenderOptions(render_opts &opts,
                                      QStringList &cpudeints)
{
    opts.renderers->append("direct3d");
    opts.deints->insert("direct3d", cpudeints);
    (*opts.osds)["direct3d"].append("softblend");
    (*opts.osds)["direct3d"].append("direct3d");
    (*opts.safe_renderers)["dummy"].append("direct3d");
    (*opts.safe_renderers)["nuppel"].append("direct3d");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("direct3d");
    if (opts.decoders->contains("libmpeg2"))
        (*opts.safe_renderers)["libmpeg2"].append("direct3d");
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append("direct3d");
    opts.priorities->insert("direct3d", 55);
}

VideoOutputD3D::VideoOutputD3D(void)
  : VideoOutput(),         m_lock(QMutex::Recursive),
    m_hWnd(NULL),          m_render(NULL),
    m_video(NULL),
    m_render_valid(false), m_render_reset(false), m_pip_active(NULL),
    m_osd_painter(NULL)
{
    m_pauseFrame.buf = NULL;
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
        delete m_render;
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

bool VideoOutputD3D::InputChanged(const QSize &input_size,
                                  float        aspect,
                                  MythCodecID  av_codec_id,
                                  void        *codec_private,
                                  bool        &aspect_only)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("InputChanged(%1,%2,%3) %4")
            .arg(input_size.width()).arg(input_size.height()).arg(aspect)
            .arg(toString(av_codec_id)));

    QMutexLocker locker(&m_lock);

    if (!codec_is_std(av_codec_id))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("New video codec is not supported."));
        errorState = kError_Unknown;
        return false;
    }

    if (input_size == window.GetVideoDim())
    {
        if (window.GetVideoAspect() != aspect)
        {
            aspect_only = true;
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        return true;
    }

    TearDown();
    QRect disp = window.GetDisplayVisibleRect();
    if (Init(input_size.width(), input_size.height(),
             aspect, m_hWnd, disp.left(), disp.top(),
             disp.width(), disp.height(), av_codec_id, m_hEmbedWnd))
    {
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR +
        QString("Failed to re-initialise video output."));
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

    VERBOSE(VB_PLAYBACK, LOC +
            "Direct3D device successfully initialized.");
    m_render_valid = true;
    return true;
}

bool VideoOutputD3D::Init(int width, int height, float aspect,
                          WId winid, int winx, int winy, int winw,
                          int winh, MythCodecID codec_id, WId embedid)
{
    MythPainter *painter = GetMythPainter();
    if (painter)
        painter->FreeResources();

    QMutexLocker locker(&m_lock);
    m_hWnd      = winid;
    m_hEmbedWnd = embedid;
    window.SetAllowPreviewEPG(true);

    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, codec_id, embedid);

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("direct3d");

    bool success = true;
    success &= SetupContext();
    InitDisplayMeasurements(width, height, false);

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);
    success &= vbuffers.CreateBuffers(FMT_YV12,
                                      window.GetVideoDim().width(),
                                      window.GetVideoDim().height());
    m_pauseFrame.height = vbuffers.GetScratchFrame()->height;
    m_pauseFrame.width  = vbuffers.GetScratchFrame()->width;
    m_pauseFrame.bpp    = vbuffers.GetScratchFrame()->bpp;
    m_pauseFrame.size   = vbuffers.GetScratchFrame()->size;
    m_pauseFrame.buf    = new unsigned char[m_pauseFrame.size + 128];
    m_pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    MoveResize();

    if (!success)
        TearDown();
    else
    {
        m_osd_painter = new MythD3D9Painter(m_render);
        if (m_osd_painter)
        {
            m_osd_painter->SetSwapControl(false);
            VERBOSE(VB_PLAYBACK, LOC + "Created D3D9 osd painter.");
        }
        else
            VERBOSE(VB_IMPORTANT, LOC + "Failed to create D3D9 osd painter.");
    }
    return success;
}

void VideoOutputD3D::PrepareFrame(VideoFrame *buffer, FrameScanType t,
                                  OSD *osd)
{
    (void)osd;
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "PrepareFrame() called while IsErrored is true.");
        return;
    }

    if (!buffer)
        buffer = vbuffers.GetScratchFrame();

    framesPlayed = buffer->frameNumber + 1;

    if (!m_render || !m_video)
        return;

    m_render_valid = m_render->Test(m_render_reset);
    if (m_render_valid)
    {
        QRect dvr = vsz_enabled ? vsz_desired_display_rect :
                                  window.GetDisplayVideoRect();
        bool ok = m_render->ClearBuffer();
        if (ok)
            ok = m_video->UpdateVertices(dvr, window.GetVideoRect(),
                                         255, true);

        if (ok)
        {
            ok = m_render->Begin();
            if (ok)
            {
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
                                m_render->DrawRect(rect, QColor(128,0,0,255));
                            }
                        }
                       (*it)->Draw();
                    }
                }
                if (osd && m_osd_painter)
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Show() called while IsErrored is true.");
        return;
    }

    if (!m_render)
        return;

    m_render_valid = m_render->Test(m_render_reset);
    if (m_render_valid)
        m_render->Present(window.IsEmbedding() ? m_hEmbedWnd : NULL);
}

void VideoOutputD3D::EmbedInWidget(int x, int y, int w, int h)
{
    if (window.IsEmbedding())
        return;

    VideoOutput::EmbedInWidget(x, y, w, h);
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

void VideoOutputD3D::UpdatePauseFrame(void)
{
    QMutexLocker locker(&m_lock);
    VideoFrame *used_frame = vbuffers.head(kVideoBuffer_used);
    if (!used_frame)
        used_frame = vbuffers.GetScratchFrame();

    CopyFrame(&m_pauseFrame, used_frame);
}

void VideoOutputD3D::UpdateFrame(VideoFrame *frame, D3D9Image *img)
{
    // TODO - add a size check
    bool hardware_conv = false;
    uint pitch = 0;
    uint8_t *buf = img->GetBuffer(hardware_conv, pitch);
    if (buf && hardware_conv)
    {
        int i;
        uint8_t *dst      = buf;
        uint8_t *src      = frame->buf;
        int chroma_width  = frame->width >> 1;
        int chroma_height = frame->height >> 1;
        int chroma_pitch  = pitch >> 1;
        for (i = 0; i < frame->height; i++)
        {
            memcpy(dst, src, frame->width);
            dst += pitch;
            src += frame->width;
        }

        dst = buf +  (frame->height * pitch);
        src = frame->buf + (frame->height * frame->width * 5/4);
        for (i = 0; i < chroma_height; i++)
        {
            memcpy(dst, src, chroma_width);
            dst += chroma_pitch;
            src += chroma_width;
        }

        dst = buf + (frame->height * pitch * 5/4);
        src = frame->buf + (frame->height * frame->width);
        for (i = 0; i < chroma_height; i++)
        {
            memcpy(dst, src, chroma_width);
            dst += chroma_pitch;
            src += chroma_width;
        }
    }
    else if (buf && !hardware_conv)
    {
        AVPicture image_in, image_out;
        avpicture_fill(&image_out, (uint8_t*)buf,
                       PIX_FMT_RGB32, frame->width, frame->height);
        image_out.linesize[0] = pitch;
        avpicture_fill(&image_in, frame->buf,
                       PIX_FMT_YUV420P, frame->width, frame->height);
        myth_sws_img_convert(&image_out, PIX_FMT_RGB32, &image_in,
                             PIX_FMT_YUV420P, frame->width, frame->height);
    }
    img->ReleaseBuffer();
}

void VideoOutputD3D::ProcessFrame(VideoFrame *frame, OSD *osd,
                                  FilterChain *filterList,
                                  const PIPMap &pipPlayers,
                                  FrameScanType scan)
{
    QMutexLocker locker(&m_lock);
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "ProcessFrame() called while IsErrored is true.");
        return;
    }

    bool deint_proc = m_deinterlacing && (m_deintFilter != NULL);

    bool pauseframe = false;
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &m_pauseFrame);
        pauseframe = true;
    }

    if (filterList)
        filterList->ProcessFrame(frame);

    bool safepauseframe = pauseframe && !IsBobDeint();
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

    if (m_render)
    {
        m_render_valid |= m_render->Test(m_render_reset);
        if (m_render_reset)
            SetupContext();
        if (m_render_valid && m_video)
            UpdateFrame(frame, m_video);
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
        VERBOSE(VB_PLAYBACK, LOC + "Initialise PiP.");
        m_pip = new D3D9Image(m_render, QSize(pipVideoWidth, pipVideoHeight), true);
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
        VERBOSE(VB_PLAYBACK, LOC + "Re-initialise PiP.");
        delete m_pip;
        m_pip = new D3D9Image(m_render, QSize(pipVideoWidth, pipVideoHeight), true);
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

    if (codec_is_std(myth_codec_id) && !getenv("NO_DIRECT3D"))
            list += "direct3d";

    return list;
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
