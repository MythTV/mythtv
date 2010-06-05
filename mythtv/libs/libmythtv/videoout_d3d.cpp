// -*- Mode: c++ -*-

#include <map>
#include <iostream>
#include <algorithm>
using namespace std;

#include "mythcontext.h"
#include "videoout_d3d.h"
#include "osd.h"
#include "osdsurface.h"
#include "filtermanager.h"
#include "fourcc.h"
#include "videodisplayprofile.h"
#include "mythmainwindow.h"
#include "myth_imgconvert.h"
#include "NuppelVideoPlayer.h"

#include "mmsystem.h"
#include "tv.h"

#undef UNICODE

extern "C" {
#include "avcodec.h"
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
    opts.priorities->insert("direct3d", 55);
}

VideoOutputD3D::VideoOutputD3D(void)
  : VideoOutput(),         m_lock(QMutex::Recursive),
    m_hWnd(NULL),          m_render(NULL),
    m_video(NULL),
    m_render_valid(false), m_render_reset(false),
    m_osd_painter(NULL),   m_osd(NULL), m_osd_ready(false),
    m_pip_active(NULL)
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

    if (m_osd_painter)
        delete m_osd_painter;
    m_osd_painter = NULL;
    if (m_osd)
        delete m_osd;
    m_osd = NULL;
    m_osd_ready = false;

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
    windows[0].SetDisplayVisibleRect(QRect(QPoint(0, 0), new_size));
    windows[0].SetDisplayAspect(
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

    if (input_size == windows[0].GetVideoDim())
    {
        if (windows[0].GetVideoAspect() != aspect)
        {
            aspect_only = true;
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        return true;
    }

    TearDown();
    QRect disp = windows[0].GetDisplayVisibleRect();
    if (Init(input_size.width(), input_size.height(),
             aspect, m_hWnd, disp.left(), disp.top(),
             disp.width(), disp.height(), m_hEmbedWnd))
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
    QSize size = windows[0].GetVideoDim();
    m_render = new MythRenderD3D9();
    if (!(m_render && m_render->Create(windows[0].GetDisplayVisibleRect().size(),
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

DisplayInfo VideoOutputD3D::GetDisplayInfo(void)
{
    DisplayInfo ret;
    HDC hdc = GetDC(m_hWnd);
    if (hdc)
    {
        int rate = GetDeviceCaps(hdc, VREFRESH);
        if (rate > 20 && rate < 200)
        {
            // see http://support.microsoft.com/kb/2006076
            switch (rate)
            {
                case 23:  ret.rate = 41708; break; // 23.976Hz
                case 29:  ret.rate = 33367; break; // 29.970Hz
                case 47:  ret.rate = 20854; break; // 47.952Hz
                case 59:  ret.rate = 16683; break; // 59.940Hz
                case 71:  ret.rate = 13903; break; // 71.928Hz
                case 119: ret.rate = 8342;  break; // 119.880Hz
                default:  ret.rate = 1000000 / rate;
            }
        }
        int width  = GetDeviceCaps(hdc, HORZSIZE);
        int height = GetDeviceCaps(hdc, VERTSIZE);
        ret.size   = QSize((uint)width, (uint)height);
        width      = GetDeviceCaps(hdc, HORZRES);
        height     = GetDeviceCaps(hdc, VERTRES);
        ret.res    = QSize((uint)width, (uint)height);
    }
    return ret;
}

bool VideoOutputD3D::Init(int width, int height, float aspect,
                          WId winid, int winx, int winy, int winw,
                          int winh, WId embedid)
{
    QMutexLocker locker(&m_lock);
    m_hWnd      = winid;
    m_hEmbedWnd = embedid;
    windows[0].SetAllowPreviewEPG(true);

    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, embedid);

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("direct3d");

    bool success = true;
    success &= SetupContext();
    InitDisplayMeasurements(width, height, false);

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);
    success &= vbuffers.CreateBuffers(windows[0].GetVideoDim().width(),
                                      windows[0].GetVideoDim().height());
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
        QRect osd_rect = GetTotalOSDBounds();
        m_osd = new D3D9Image(m_render, osd_rect.size());
        m_osd_painter = new MythD3D9Painter(m_render);
        if (m_osd_painter && m_osd)
        {
            m_osd->UpdateVertices(osd_rect, osd_rect);
            m_osd_painter->SetTarget(m_osd);
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
                                  windows[0].GetDisplayVideoRect();
        bool ok = m_render->ClearBuffer();
        if (ok)
            ok = m_video->UpdateVertices(dvr, windows[0].GetVideoRect(),
                                         255, true);

        if (ok)
        {
            ok = m_render->Begin();
            if (ok)
            {
                m_video->Draw();
                QMap<NuppelVideoPlayer*,D3D9Image*>::iterator it = m_pips.begin();
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
                if (m_osd_ready)
                    m_osd->Draw();
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
        m_render->Present(windows[0].IsEmbedding() ? m_hEmbedWnd : NULL);
}

void VideoOutputD3D::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    if (windows[0].IsEmbedding())
        return;

    VideoOutput::EmbedInWidget(x, y, w, h);
    m_hEmbedWnd = wid;
}

void VideoOutputD3D::StopEmbedding(void)
{
    if (!windows[0].IsEmbedding())
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

    if (!windows[0].IsEmbedding())
        ShowPIPs(frame, pipPlayers);

    if ((!pauseframe || safepauseframe) &&
        deint_proc && !m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    m_osd_ready = false;
    if (m_render)
    {
        m_render_valid |= m_render->Test(m_render_reset);
        if (m_render_reset)
            SetupContext();
        if (m_render_valid && m_video)
            UpdateFrame(frame, m_video);
        if (m_render_valid && osd && m_osd_painter)
            m_osd_ready = osd->DrawDirect(m_osd_painter,
                                          GetTotalOSDBounds().size());
    }
}

void VideoOutputD3D::ShowPIP(VideoFrame        *frame,
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

    if ((pipVideoAspect <= 0) || !pipimage ||
        !pipimage->buf || (pipimage->codec != FMT_YV12) || !pipVisible)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(loc, pipplayer);
    QRect dvr = windows[0].GetDisplayVisibleRect();

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

void VideoOutputD3D::RemovePIP(NuppelVideoPlayer *pipplayer)
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
