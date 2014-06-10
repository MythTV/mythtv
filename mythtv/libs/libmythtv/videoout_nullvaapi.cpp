#include "videoout_nullvaapi.h"
#include "vaapicontext.h"

#define LOC QString("NullVAAPI: ")

void VideoOutputNullVAAPI::GetRenderOptions(render_opts &opts)
{
    opts.renderers->append("nullvaapi");
    (*opts.osds)["nullvaapi"].append("dummy");
    QStringList dummy(QString("dummy"));
    opts.deints->insert("nullvaapi", dummy);
    if (opts.decoders->contains("vaapi"))
        (*opts.safe_renderers)["vaapi"].append("nullvaapi");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("nullvaapi");
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append("nullvaapi");
    (*opts.safe_renderers)["dummy"].append("nullvaapi");
    (*opts.safe_renderers)["nuppel"].append("nullvaapi");

    opts.priorities->insert("nullvaapi", 20);
}

VideoOutputNullVAAPI::VideoOutputNullVAAPI()
  : m_ctx(NULL), m_lock(QMutex::Recursive), m_shadowBuffers(NULL)
{
}

VideoOutputNullVAAPI::~VideoOutputNullVAAPI()
{
    TearDown();
}

void VideoOutputNullVAAPI::TearDown(void)
{
    QMutexLocker lock(&m_lock);
    DeleteBuffers();
    DeleteVAAPIContext();
}

bool VideoOutputNullVAAPI::CreateVAAPIContext(QSize size)
{
    QMutexLocker lock(&m_lock);
    if (m_ctx)
        DeleteVAAPIContext();

    m_ctx = new VAAPIContext(kVADisplayX11, video_codec_id);
    if (m_ctx && m_ctx->CreateDisplay(size, true))
        return true;
    return false;
}

void VideoOutputNullVAAPI::DeleteVAAPIContext(void)
{
    QMutexLocker lock(&m_lock);
    delete m_ctx;
    m_ctx = NULL;
}

bool VideoOutputNullVAAPI::InitBuffers(void)
{
    QMutexLocker lock(&m_lock);
    if (!codec_is_vaapi_hw(video_codec_id) || !m_ctx ||
        !m_ctx->CreateBuffers())
    {
        return false;
    }

    // create VAAPI buffers
    vbuffers.Init(24, true, 2, 1, 4, 1);
    int num_buffers = m_ctx->GetNumBuffers();
    const QSize video_dim = window.GetActualVideoDim();
    bool ok = true;
    for (int i = 0; i < num_buffers; i++)
    {
        ok &= vbuffers.CreateBuffer(video_dim.width(),
                                    video_dim.height(), i,
                                    m_ctx->GetVideoSurface(i),
                                    FMT_VAAPI);
    }

    // create CPU buffers
    m_shadowBuffers = new VideoBuffers();
    if (!m_shadowBuffers)
        return false;
    m_shadowBuffers->Init(24, true, 2, 1, 4, 1);
    if (!m_shadowBuffers->CreateBuffers(FMT_YV12,
                                        video_dim.width(),
                                        video_dim.height()))
    {
        return false;
    }

    return true;
}

void VideoOutputNullVAAPI::DeleteBuffers(void)
{
    QMutexLocker lock(&m_lock);
    DiscardFrames(true);
    vbuffers.Reset();
    vbuffers.DeleteBuffers();
    if (m_shadowBuffers)
    {
        m_shadowBuffers->Reset();
        m_shadowBuffers->DeleteBuffers();
    }
    delete m_shadowBuffers;
    m_shadowBuffers = NULL;
}

QStringList VideoOutputNullVAAPI::GetAllowedRenderers(MythCodecID myth_codec_id)
{
    QStringList list;
    if ((codec_is_vaapi_hw(myth_codec_id)) && !getenv("NO_VAAPI"))
        list += "nullvaapi";
    return list;
}

bool VideoOutputNullVAAPI::Init(const QSize &video_dim_buf,
                                const QSize &video_dim_disp,
                                float aspect,
                                WId winid, const QRect &win_rect,
                                MythCodecID codec_id)
{
    QMutexLocker locker(&m_lock);
    bool ok = VideoOutput::Init(video_dim_buf, video_dim_disp,
                                aspect, winid, win_rect, codec_id);
    if (!codec_is_vaapi_hw(video_codec_id))
        return false;

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("nullvaapi");
    if (ok) ok = CreateVAAPIContext(window.GetActualVideoDim());
    if (ok) ok = InitBuffers();
    if (!ok)
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        "Created VAAPI context with GPU decoding");
    return ok;
}

bool VideoOutputNullVAAPI::InputChanged(const QSize &video_dim_buf,
                                        const QSize &video_dim_disp,
                                        float aspect,
                                        MythCodecID av_codec_id,
                                        void *codec_private,
                                        bool &aspect_only)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("InputChanged(%1,%2,%3) '%4'->'%5'")
            .arg(video_dim_disp.width()).arg(video_dim_disp.height())
            .arg(aspect)
            .arg(toString(video_codec_id)).arg(toString(av_codec_id)));

    QMutexLocker locker(&m_lock);

    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = video_dim_disp != window.GetActualVideoDim();
    if (!res_changed && !cid_changed)
    {
        aspect_only = true;
        return true;
    }

    TearDown();
    QRect disp = window.GetDisplayVisibleRect();
    if (Init(video_dim_buf, video_dim_disp,
             aspect, 0, disp, av_codec_id))
    {
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to re-initialise video output.");
    errorState = kError_Unknown;
    return false;
}

void* VideoOutputNullVAAPI::GetDecoderContext(unsigned char* buf, uint8_t*& id)
{
    if (m_ctx)
    {
        id = m_ctx->GetSurfaceIDPointer(buf);
        return &m_ctx->m_ctx;
    }
    return NULL;
}

// Always returns the CPU version of a frame
VideoFrame* VideoOutputNullVAAPI::GetLastDecodedFrame(void)
{
    VideoFrame* gpu = vbuffers.GetLastDecodedFrame();
    for (uint i = 0; i < vbuffers.Size(); i++)
        if (vbuffers.At(i) == gpu)
            return m_shadowBuffers->At(i);
    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find frame.");
    return NULL;
}

// Always returns the CPU version of a frame
VideoFrame* VideoOutputNullVAAPI::GetLastShownFrame(void)
{
    VideoFrame* gpu = vbuffers.GetLastShownFrame();
    for (uint i = 0; i < vbuffers.Size(); i++)
        if (vbuffers.At(i) == gpu)
            return m_shadowBuffers->At(i);
    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find frame.");
    return NULL;
}

// Should work with either the CPU or GPU version of a frame
void VideoOutputNullVAAPI::DiscardFrame(VideoFrame *frame)
{
    // is this a CPU frame
    for (uint i = 0; i < m_shadowBuffers->Size(); i++)
    {
        if (m_shadowBuffers->At(i) == frame)
        {
            frame = vbuffers.At(i);
            break;
        }
    }

    // is this a GPU frame
    for (uint i = 0; i < vbuffers.Size(); i++)
    {
        if (vbuffers.At(i) == frame)
        {
            m_lock.lock();
            vbuffers.DiscardFrame(frame);
            m_lock.unlock();
            return;
        }
    }
}

// Should work with either the CPU or GPU version of a frame
void VideoOutputNullVAAPI::DoneDisplayingFrame(VideoFrame *frame)
{
    // is this a CPU frame
    for (uint i = 0; i < m_shadowBuffers->Size(); i++)
    {
        if (m_shadowBuffers->At(i) == frame)
        {
            frame = vbuffers.At(i);
            break;
        }
    }

    // is this a GPU frame
    for (uint i = 0; i < vbuffers.Size(); i++)
    {
        if (vbuffers.At(i) == frame)
        {
            m_lock.lock();
            VideoOutput::DiscardFrame(frame);
            m_lock.unlock();
            return;
        }
    }
}

void VideoOutputNullVAAPI::ReleaseFrame(VideoFrame *frame)
{
    if (!frame)
        return;

    if ((frame->codec != FMT_VAAPI) || !m_ctx)
    {
        VideoOutput::ReleaseFrame(frame);
        return;
    }

    QMutexLocker lock(&m_lock);
    for (uint i = 0; i < vbuffers.Size() && m_ctx; i++)
    {
        if (vbuffers.At(i)->buf == frame->buf)
        {
            VideoFrame *vf = m_shadowBuffers->At(i);
            vf->aspect = frame->aspect;
            vf->disp_timecode = frame->disp_timecode;
            vf->dummy = frame->dummy;
            vf->frameNumber = frame->frameNumber;
            vf->interlaced_frame = frame->interlaced_frame;
            vf->timecode = frame->timecode;
            vf->repeat_pict = frame->repeat_pict;
            vf->top_field_first = frame->top_field_first;
            m_ctx->CopySurfaceToFrame(vf, vbuffers.At(i)->buf);
        }
    }
    VideoOutput::ReleaseFrame(frame);
}
