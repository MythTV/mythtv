#include "videoout_omx.h"

#include <cassert>
#include <cstddef>
#include <vector>
#include <algorithm> // max/min

#include <IL/OMX_Core.h>
#include <IL/OMX_Video.h>
#ifdef USING_BROADCOM
#include <IL/OMX_Broadcom.h>
#endif

#include "filtermanager.h"
#include "videodisplayprofile.h"
#include "videobuffers.h"

#include "privatedecoder_omx.h" // For PrivateDecoderOMX::s_name
#include "omxcontext.h"
using namespace omxcontext;


/*
 * Macros
 */
#define LOC QString("VOMX:%1 ").arg(m_render.Id())

// Roundup a value: y = ROUNDUP(x,4)
#define ROUNDUP( _x,_z) ((_x) + ((-(int)(_x)) & ((_z) -1)) )

// VideoFrame <> OMX_BUFFERHEADERTYPE
#define FRAMESETHDR(f,h) ((f)->priv[3] = reinterpret_cast<unsigned char* >(h))
#define FRAME2HDR(f) ((OMX_BUFFERHEADERTYPE*)((f)->priv[3]))
#define HDR2FRAME(h) ((VideoFrame*)((h)->pAppPrivate))


/*
 * Types
 */


/*
 * Constants
 */
const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 12;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;

QString const VideoOutputOMX::kName ="openmax";


/*
 * Functions
 */

// static
void VideoOutputOMX::GetRenderOptions(render_opts &opts,
                                      QStringList &cpudeints)
{
    opts.renderers->append(kName);
    opts.deints->insert(kName, cpudeints);
    (*opts.osds)[kName].append("softblend");

    (*opts.safe_renderers)["dummy"].append(kName);
    (*opts.safe_renderers)["nuppel"].append(kName);
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append(kName);
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append(kName);
    if (opts.decoders->contains(PrivateDecoderOMX::s_name))
        (*opts.safe_renderers)[PrivateDecoderOMX::s_name].append(kName);

    opts.priorities->insert(kName, 70);
}

// static
QStringList VideoOutputOMX::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &)
{
    QStringList list;
    if (codec_is_std(myth_codec_id))
        list += kName;

    return list;
}

VideoOutputOMX::VideoOutputOMX() : m_render("video_render", *this)
{
    init(&av_pause_frame, FMT_YV12, NULL, 0, 0, 0);

    if (OMX_ErrorNone != m_render.Init(OMX_IndexParamVideoInit))
        return;

    if (!m_render.IsValid())
        return;

    // Show default port definitions and video formats supported
    for (int port = 0; port < m_render.Ports(); ++port)
    {
        m_render.ShowPortDef(port, LOG_DEBUG);
        if (0) m_render.ShowFormats(port, LOG_DEBUG);
    }
}

// virtual
VideoOutputOMX::~VideoOutputOMX()
{
    // Must shutdown the decoder now before our state becomes invalid.
    // When the decoder dtor is called our state has already been destroyed.
    m_render.Shutdown();

    DeleteBuffers();
}

// virtual
bool VideoOutputOMX::Init(          // Return true if successful
    const QSize &video_dim_buf,     // video buffer size
    const QSize &video_dim_disp,    // video display size
    float aspect,                   // w/h of presented video
    WId winid,                      // "video playback window" widget winId()
    const QRect &win_rect,          // playback rect on "video playback window"
    MythCodecID codec_id )          // video codec
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + __func__ +
        QString(" vbuf=%1x%2 vdisp=%3x%4 aspect=%5 win=%6,%7,%8x%9 codec=%10")
            .arg(video_dim_buf.width()).arg(video_dim_buf.height())
            .arg(video_dim_disp.width()).arg(video_dim_disp.height())
            .arg(aspect).arg(win_rect.x()).arg(win_rect.y())
            .arg(win_rect.width()).arg(win_rect.height())
            .arg(toString(codec_id)) );

    if (!codec_is_std(codec_id))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Cannot create VideoOutput for codec %1")
            .arg(toString(codec_id)));
        errorState = kError_Unknown;
        return false;
    }

    if (getenv("NO_OPENMAX"))
    {
        LOG(VB_PLAYBACK, LOG_NOTICE, LOC + __func__ + " OpenMAX disabled");
        errorState = kError_Unknown;
        return false;
    }

    if (!m_render.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No video render");
        errorState = kError_Unknown;
        return false;
    }

    window.SetAllowPreviewEPG(true);

    if (!VideoOutput::Init(video_dim_buf, video_dim_disp,
                      aspect, winid, win_rect, codec_id))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + ": VideoOutput::Init failed");
        return false;
    }

    // Set resolution/measurements
    InitDisplayMeasurements(video_dim_disp.width(), video_dim_disp.height(), false);

    // Setup video buffers
    vbuffers.Init(std::max(OMX_U32(kNumBuffers), m_render.PortDef().nBufferCountMin),
                  true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);
    if (!CreateBuffers(video_dim_buf, video_dim_disp, winid))
        return false;

    // Goto OMX_StateIdle & allocate all buffers
    OMXComponentCB<VideoOutputOMX> cb(this, &VideoOutputOMX::UseBuffersCB);
    OMX_ERRORTYPE e = m_render.SetState(OMX_StateIdle, 500, &cb);
    if (e != OMX_ErrorNone)
        return false;

    // Goto OMX_StateExecuting
    e = m_render.SetState(OMX_StateExecuting, 500);
    if (e != OMX_ErrorNone)
        return false;

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer(kName);

    MoveResize();

    return true;
}

// virtual
bool VideoOutputOMX::InputChanged(  // Return true if successful
    const QSize &video_dim_buf,     // video buffer size
    const QSize &video_dim_disp,    // video display size
    float        aspect,            // w/h of presented video
    MythCodecID  av_codec_id,       // video codec
    void        *codec_private,
    bool        &aspect_only )      // Out: true if aspect only changed
{
    QSize cursize = window.GetActualVideoDim();

    LOG(VB_PLAYBACK, LOG_INFO, LOC + __func__ +
        QString(" from %1: %2x%3 aspect %4 to %5: %6x%7 aspect %9")
            .arg(toString(video_codec_id)).arg(cursize.width())
            .arg(cursize.height()).arg(window.GetVideoAspect())
            .arg(toString(av_codec_id)).arg(video_dim_disp.width())
            .arg(video_dim_disp.height()).arg(aspect));

    if (!codec_is_std(av_codec_id))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "New video codec is not supported.");
        errorState = kError_Unknown;
        return false;
    }

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

    VideoOutput::InputChanged(video_dim_buf, video_dim_disp,
                              aspect, av_codec_id, codec_private,
                              aspect_only);

    OMXComponentCB<VideoOutputOMX> cb(this, &VideoOutputOMX::FreeBuffersCB);
    OMX_ERRORTYPE e;
    e = m_render.PortDisable(0, 500, &cb);
    if (e != OMX_ErrorNone)
        return false;

    DeleteBuffers();
    if (!CreateBuffers(video_dim_buf, video_dim_disp))
        return false;

    // Re-enable port and allocate new buffers
    OMXComponentCB<VideoOutputOMX> cb2(this, &VideoOutputOMX::UseBuffersCB);
    e = m_render.PortEnable(0, 500, &cb2);
    if (e != OMX_ErrorNone)
        return false;

    MoveResize();

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer(kName);

    return true;
}

// virtual
bool VideoOutputOMX::ApproveDeintFilter(const QString& filtername) const
{
    if (codec_is_std(video_codec_id))
    {
        return !filtername.contains("bobdeint") &&
               !filtername.contains("opengl") &&
               !filtername.contains("vdpau");
    }

    return false;
}

// virtual
void VideoOutputOMX::EmbedInWidget(const QRect &rect)
{
    if (!window.IsEmbedding())
        VideoOutput::EmbedInWidget(rect);
}

// virtual
void VideoOutputOMX::StopEmbedding(void)
{
    if (window.IsEmbedding())
        VideoOutput::StopEmbedding();
}

// virtual
void VideoOutputOMX::Zoom(ZoomDirection direction)
{
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputOMX::CreatePauseFrame(void)
{
    delete [] av_pause_frame.buf;
    av_pause_frame.buf = NULL;

    VideoFrame *scratch = vbuffers.GetScratchFrame();

    init(&av_pause_frame, FMT_YV12, new unsigned char[scratch->size + 128],
         scratch->width, scratch->height, scratch->size);

    av_pause_frame.frameNumber = scratch->frameNumber;

    clear(&av_pause_frame);

    // Need two pause frames for OMX video_render.
    // 1st is held by EmptyBuffer until next frame is sent.
    // Need additional pause frame for advanced deinterlacer
    while (vbuffers.Size(kVideoBuffer_pause) < 3)
        vbuffers.Enqueue(kVideoBuffer_pause, vbuffers.Dequeue(kVideoBuffer_avail));
}

// pure virtual
void VideoOutputOMX::UpdatePauseFrame(int64_t &disp_timecode)
{
    if (!av_pause_frame.buf)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " - no buffers?");
        return;
    }

    // Try used frame first, then fall back to scratch frame.
    vbuffers.begin_lock(kVideoBuffer_used);

    VideoFrame *used_frame = (vbuffers.Size(kVideoBuffer_used) > 0) ?
                                vbuffers.Head(kVideoBuffer_used) : NULL;
    if (used_frame)
        CopyFrame(&av_pause_frame, used_frame);

    vbuffers.end_lock();

    if (!used_frame)
    {
        used_frame = vbuffers.Tail(kVideoBuffer_pause);
        used_frame->frameNumber = framesPlayed - 1;
        CopyFrame(&av_pause_frame, used_frame);
    }

    disp_timecode = av_pause_frame.disp_timecode;
}

/**
 * Draw OSD, apply filters and deinterlacing,
 */
// pure virtual
void VideoOutputOMX::ProcessFrame(VideoFrame *frame, OSD *osd,
                                  FilterChain *filterList,
                                  const PIPMap &pipPlayers,
                                  FrameScanType scan)
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " IsErrored is true");
        return;
    }

    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(frame, &av_pause_frame);
    }

    CropToDisplay(frame);

    if (filterList)
        filterList->ProcessFrame(frame);

    if (m_deinterlacing && m_deintFilter && m_deinterlaceBeforeOSD)
        m_deintFilter->ProcessFrame(frame, scan);

    ShowPIPs(frame, pipPlayers);
    if (osd && !window.IsEmbedding())
        DisplayOSD(frame, osd);

    if (m_deinterlacing && m_deintFilter && !m_deinterlaceBeforeOSD)
        m_deintFilter->ProcessFrame(frame, scan);
}

// tells show what frame to be show, do other last minute stuff
// pure virtual
void VideoOutputOMX::PrepareFrame(VideoFrame *buffer, FrameScanType scan, OSD *osd)
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " IsErrored is true");
        return;
    }

    if (!buffer)
    {
        buffer = vbuffers.GetScratchFrame();
        vbuffers.SetLastShownFrameToScratch();
        // Rotate pause frames
        vbuffers.Enqueue(kVideoBuffer_pause, vbuffers.Dequeue(kVideoBuffer_pause));
    }

    framesPlayed = buffer->frameNumber + 1;

    SetVideoRect(window.GetDisplayVideoRect(), window.GetVideoRect() );
}

// BLT the last prepared frame to the screen as quickly as possible.
// pure virtual
void VideoOutputOMX::Show(FrameScanType scan)
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " IsErrored is true");
        return;
    }

    VideoFrame *frame = GetLastShownFrame();
    if (!frame)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No LastShownFrame");
        return;
    }

    OMX_BUFFERHEADERTYPE *hdr = FRAME2HDR(frame);
    if (!hdr)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " Frame has no buffer header");
        return;
    }

    assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
    assert(hdr->nVersion.nVersion == OMX_VERSION);
    assert(hdr->pBuffer == frame->buf);

    if (hdr->nFilledLen)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + __func__ + " Frame in use");
        return; // Pending empty callback
    }

    assert(frame->codec == FMT_YV12);
    hdr->nFilledLen = frame->offsets[2] + (frame->offsets[1] >> 2);
    hdr->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
    OMX_ERRORTYPE e = OMX_EmptyThisBuffer(m_render.Handle(), hdr);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
            "OMX_EmptyThisBuffer error %1").arg(Error2String(e)) );
        hdr->nFlags = 0;
        hdr->nFilledLen = 0;
    }
}

// virtual
void VideoOutputOMX::MoveResizeWindow(QRect r)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + __func__ + QString(" rect=%1,%2,%3x%4")
        .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()) );
}

// virtual
void VideoOutputOMX::DrawUnusedRects(bool sync)
{
    (void)sync;
}

bool VideoOutputOMX::CreateBuffers(
    const QSize &video_dim_buf,     // video buffer size
    const QSize &video_dim_disp,    // video display size
    WId winid )
{
    // Set the video dimensions
    OMX_PARAM_PORTDEFINITIONTYPE def = m_render.PortDef();
    assert(vbuffers.Size() >= def.nBufferCountMin);
    def.nBufferCountActual = vbuffers.Size();
    def.format.video.cMIMEType = NULL;
    if (winid)
        def.format.video.pNativeRender = NULL;
    def.format.video.nFrameWidth = video_dim_disp.width();
    def.format.video.nFrameHeight = video_dim_disp.height();
    def.format.video.nStride = ROUNDUP(video_dim_buf.width(), 32);
    def.format.video.nSliceHeight = video_dim_buf.height();
    def.format.video.nBitrate = 0;
    def.format.video.xFramerate = 0;
    def.format.video.bFlagErrorConcealment = OMX_FALSE;
    def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    def.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
    if (winid)
        def.format.video.pNativeWindow = OMX_NATIVE_WINDOWTYPE(winid);
    OMX_ERRORTYPE e = m_render.SetParameter(OMX_IndexParamPortDefinition, &def);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "Set OMX_IndexParamPortDefinition error %1")
            .arg(Error2String(e)));
        return false;
    }

    // Update port status with revised buffer size etc
    if (OMX_ErrorNone != m_render.GetPortDef())
        return false;

#ifdef USING_BROADCOM
    // Setup output
    OMX_CONFIG_DISPLAYREGIONTYPE dregion;
    OMX_DATA_INIT(dregion);
    dregion.nPortIndex = m_render.Base();
    dregion.set = OMX_DISPLAYSETTYPE( OMX_DISPLAY_SET_FULLSCREEN |
                    OMX_DISPLAY_SET_TRANSFORM |
                    OMX_DISPLAY_SET_MODE |
                    OMX_DISPLAY_SET_PIXEL |
                    OMX_DISPLAY_SET_NOASPECT |
                    OMX_DISPLAY_SET_LAYER );
    dregion.fullscreen = OMX_FALSE;
    dregion.transform = OMX_DISPLAY_ROT0;
    dregion.noaspect = OMX_TRUE;
    dregion.mode = OMX_DISPLAY_MODE_FILL; //OMX_DISPLAY_MODE_LETTERBOX;
    dregion.pixel_x = dregion.pixel_y = 0;
    // NB Qt EGLFS uses layer 1 - See createDispmanxLayer() in
    // mkspecs/devices/linux-rasp-pi-g++/qeglfshooks_pi.cpp.
    // Therefore to view video must select layer >= 1
    dregion.layer = 2;

    e = m_render.SetConfig(OMX_IndexConfigDisplayRegion, &dregion);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "Set OMX_IndexConfigDisplayRegion error %1")
            .arg(Error2String(e)));
        return false;
    }
#endif // USING_BROADCOM

    if (!SetVideoRect(window.GetDisplayVideoRect(), window.GetVideoRect()))
        return false;

    const OMX_VIDEO_PORTDEFINITIONTYPE &vdef = m_render.PortDef().format.video;
    const OMX_U32 nBufferSize = m_render.PortDef().nBufferSize;
    int pitches[3], offsets[3];
    pitches[0] = vdef.nStride;
    pitches[1] = pitches[2] = vdef.nStride >> 1;
    offsets[0] = 0;
    offsets[1] = vdef.nStride * vdef.nSliceHeight;
    offsets[2] = offsets[1] + (offsets[1] >> 2);

    std::vector<unsigned char*> bufs;
    std::vector<YUVInfo> yuvinfo;
    for (uint i = 0; i < vbuffers.Size(); ++i)
    {
        yuvinfo.push_back(YUVInfo(vdef.nFrameWidth, vdef.nFrameHeight,
                            nBufferSize, pitches, offsets));
        void *buf = av_malloc(nBufferSize + 64);
        if (!buf)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Out of memory");
            errorState = kError_Unknown;
            return false;
        }
        m_bufs.push_back(buf);
        bufs.push_back((unsigned char *)buf);
    }
    if (!vbuffers.CreateBuffers(FMT_YV12, vdef.nFrameWidth,
                                vdef.nFrameHeight, bufs, yuvinfo))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "CreateBuffers failed");
        errorState = kError_Unknown;
        return false;
    }

    CreatePauseFrame();

    return true;
}

void VideoOutputOMX::DeleteBuffers()
{
    delete [] av_pause_frame.buf;
    init(&av_pause_frame, FMT_YV12, NULL, 0, 0, 0);

    vbuffers.DeleteBuffers();

    while (!m_bufs.empty())
    {
        av_free(m_bufs.back());
        m_bufs.pop_back();
    }
}

bool VideoOutputOMX::SetVideoRect(const QRect &disp_rect, const QRect &vid_rect)
{
    if (disp_rect == m_disp_rect && vid_rect == m_vid_rect)
        return true;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + __func__ + QString(
            " display=%1,%2,%3x%4 (%9) video=%5,%6,%7x%8 (%10)")
        .arg(disp_rect.x()).arg(disp_rect.y())
        .arg(disp_rect.width()).arg(disp_rect.height())
        .arg(vid_rect.x()).arg(vid_rect.y())
        .arg(vid_rect.width()).arg(vid_rect.height())
        .arg(window.GetDisplayAspect()).arg(window.GetOverridenVideoAspect()) );

#ifdef USING_BROADCOM
    OMX_CONFIG_DISPLAYREGIONTYPE dregion;
    OMX_DATA_INIT(dregion);
    dregion.nPortIndex = m_render.Base();
    dregion.set = OMX_DISPLAYSETTYPE(
                    OMX_DISPLAY_SET_DEST_RECT | OMX_DISPLAY_SET_SRC_RECT);
    dregion.dest_rect.x_offset = disp_rect.x();
    dregion.dest_rect.y_offset = disp_rect.y();
    dregion.dest_rect.width    = disp_rect.width();
    dregion.dest_rect.height   = disp_rect.height();
    dregion.src_rect.x_offset = vid_rect.x();
    dregion.src_rect.y_offset = vid_rect.y();
    dregion.src_rect.width    = vid_rect.width();
    dregion.src_rect.height   = vid_rect.height();

    OMX_ERRORTYPE e = m_render.SetConfig(OMX_IndexConfigDisplayRegion, &dregion);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "Set OMX_IndexConfigDisplayRegion error %1")
            .arg(Error2String(e)));
        return false;
    }
#endif // USING_BROADCOM

    m_disp_rect = disp_rect;
    m_vid_rect = vid_rect;

    return true;
}

// virtual
OMX_ERRORTYPE VideoOutputOMX::EmptyBufferDone(
    OMXComponent&, OMX_BUFFERHEADERTYPE *hdr)
{
    assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
    assert(hdr->nVersion.nVersion == OMX_VERSION);
    hdr->nFilledLen = 0;
    hdr->nFlags = 0;
    return OMX_ErrorNone;
}

// Shutdown OMX_StateIdle -> OMX_StateLoaded callback
// virtual
void VideoOutputOMX::ReleaseBuffers(OMXComponent &)
{
    for (uint i = 0; i < vbuffers.Size(); ++i)
    {
        VideoFrame *vf = vbuffers.At(i);
        assert(vf);
        OMX_BUFFERHEADERTYPE *hdr = FRAME2HDR(vf);
        assert(hdr);
        assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
        assert(hdr->nVersion.nVersion == OMX_VERSION);
        assert(vf == HDR2FRAME(hdr));

        OMX_ERRORTYPE e = OMX_FreeBuffer(m_render.Handle(), m_render.Base(), hdr);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                    "OMX_FreeBuffer 0x%1 error %2")
                .arg(quintptr(hdr),0,16).arg(Error2String(e)));
        }
    }
}

// Use frame buffers
OMX_ERRORTYPE VideoOutputOMX::UseBuffersCB()
{
    const OMX_PARAM_PORTDEFINITIONTYPE &def = m_render.PortDef();
    assert(vbuffers.Size() == def.nBufferCountActual);

    OMX_ERRORTYPE e = OMX_ErrorNone;

    for (uint i = 0; i < vbuffers.Size(); ++i)
    {
        VideoFrame *vf = vbuffers.At(i);
        assert(OMX_U32(vf->size) >= def.nBufferSize);
        assert(vf->buf);

        OMX_BUFFERHEADERTYPE *hdr;
        e = OMX_UseBuffer(m_render.Handle(), &hdr, m_render.Base(), vf, def.nBufferSize, vf->buf);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "OMX_UseBuffer error %1").arg(Error2String(e)) );
            break;
        }
        if (hdr->nSize != sizeof(OMX_BUFFERHEADERTYPE))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_UseBuffer header mismatch");
            OMX_FreeBuffer(m_render.Handle(), m_render.Base(), hdr);
            e = OMX_ErrorVersionMismatch;
            break;
        }
        if (hdr->nVersion.nVersion != OMX_VERSION)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_UseBuffer version mismatch");
            OMX_FreeBuffer(m_render.Handle(), m_render.Base(), hdr);
            e = OMX_ErrorVersionMismatch;
            break;
        }
        assert(vf == HDR2FRAME(hdr));
        FRAMESETHDR(vf, hdr);
        hdr->nFilledLen = 0;
        hdr->nOffset = 0;
    }

    return e;
}

// Free all OMX buffers
// OMX_CommandPortDisable callback
OMX_ERRORTYPE VideoOutputOMX::FreeBuffersCB()
{
    assert(vbuffers.Size() == m_render.PortDef().nBufferCountActual);

    OMX_ERRORTYPE e = OMX_ErrorNone;

    for (uint i = 0; i < vbuffers.Size(); ++i)
    {
        VideoFrame *vf = vbuffers.At(i);
        assert(vf);
        OMX_BUFFERHEADERTYPE *hdr = FRAME2HDR(vf);
        assert(hdr);
        assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
        assert(hdr->nVersion.nVersion == OMX_VERSION);
        assert(vf == HDR2FRAME(hdr));

        e = OMX_FreeBuffer(m_render.Handle(), m_render.Base(), hdr);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                    "OMX_FreeBuffer 0x%1 error %2")
                .arg(quintptr(hdr),0,16).arg(Error2String(e)));
            break;
        }
    }
    return e;
}
// EOF
