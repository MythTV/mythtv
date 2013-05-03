#include "videoout_openglvaapi.h"
#include "vaapicontext.h"

#define LOC QString("VidOutGLVAAPI: ")
#define ERR QString("VidOutGLVAAPI Error: ")

void VideoOutputOpenGLVAAPI::GetRenderOptions(render_opts &opts)
{
    opts.renderers->append("openglvaapi");

    (*opts.deints)["openglvaapi"].append("vaapionefield");
    (*opts.deints)["openglvaapi"].append("vaapibobdeint");
    (*opts.deints)["openglvaapi"].append("none");
    (*opts.osds)["openglvaapi"].append("opengl2");

    if (opts.decoders->contains("vaapi"))
        (*opts.safe_renderers)["vaapi"].append("openglvaapi");

    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("openglvaapi");

    (*opts.safe_renderers)["dummy"].append("openglvaapi");
    (*opts.safe_renderers)["nuppel"].append("openglvaapi");

    opts.priorities->insert("openglvaapi", 110);
}

VideoOutputOpenGLVAAPI::VideoOutputOpenGLVAAPI()
  : VideoOutputOpenGL(), m_ctx(NULL), m_pauseBuffer(NULL)
{
}

VideoOutputOpenGLVAAPI::~VideoOutputOpenGLVAAPI()
{
    TearDown();
}

void VideoOutputOpenGLVAAPI::TearDown(void)
{
    DeleteVAAPIContext();
}

bool VideoOutputOpenGLVAAPI::InputChanged(const QSize &video_dim_buf,
                                          const QSize &video_dim_disp,
                                          float aspect,
                              MythCodecID  av_codec_id, void *codec_private,
                              bool &aspect_only)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("InputChanged(%1,%2,%3) %4->%5")
            .arg(video_dim_disp.width()).arg(video_dim_disp.height())
            .arg(aspect)
            .arg(toString(video_codec_id)).arg(toString(av_codec_id)));

    if (!codec_is_vaapi(av_codec_id))
        return VideoOutputOpenGL::InputChanged(video_dim_buf, video_dim_disp,
                                               aspect, av_codec_id,
                                               codec_private, aspect_only);
                                                   
    QMutexLocker locker(&gl_context_lock);

    bool wasembedding = window.IsEmbedding();
    QRect oldrect;
    if (wasembedding)
    {
        oldrect = window.GetEmbeddingRect();
        StopEmbedding();
    }

    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = video_dim_disp != window.GetActualVideoDim();
    bool asp_changed = aspect      != window.GetVideoAspect();
    
    if (!res_changed && !cid_changed)
    {
        if (asp_changed)
        {
            aspect_only = true;
            VideoAspectRatioChanged(aspect);
            MoveResize();
            if (wasembedding)
                EmbedInWidget(oldrect);
        }
        return true;
    }

    if (gCoreContext->IsUIThread())
        TearDown();
    else
        DestroyCPUResources();

    QRect disp = window.GetDisplayVisibleRect();
    if (Init(video_dim_buf, video_dim_disp,
             aspect, gl_parent_win, disp, av_codec_id))
    {
        if (wasembedding)
            EmbedInWidget(oldrect);
        if (gCoreContext->IsUIThread())
            BestDeint();
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to re-initialise video output.");
    errorState = kError_Unknown;

    return false;
}

bool VideoOutputOpenGLVAAPI::Init(const QSize &video_dim_buf,
                                  const QSize &video_dim_disp,
                                  float aspect,
                                  WId winid, const QRect &win_rect,
                                  MythCodecID codec_id)
{
    bool ok = VideoOutputOpenGL::Init(video_dim_buf, video_dim_disp,
                                      aspect, winid,
                                      win_rect, codec_id);
    if (ok && codec_is_vaapi(video_codec_id))
        return CreateVAAPIContext(window.GetActualVideoDim());
    return ok;
}

bool VideoOutputOpenGLVAAPI::CreateVAAPIContext(QSize size)
{
    // FIXME During a video stream change this is called from the decoder
    // thread - which breaks all other efforts to remove non-UI thread
    // access to the OpenGL context. There is no obvious fix however - if we
    // don't delete and re-create the VAAPI decoder context immediately then
    // the decoder fails and playback exits.
    OpenGLLocker ctx_lock(gl_context);

    if (m_ctx)
        DeleteVAAPIContext();

    m_ctx = new VAAPIContext(kVADisplayGLX, video_codec_id);
    if (m_ctx && m_ctx->CreateDisplay(size) && m_ctx->CreateBuffers())
    {
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
        InitPictureAttributes();
        return ok;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI context.");
    errorState = kError_Unknown;
    return false;
}

void VideoOutputOpenGLVAAPI::DeleteVAAPIContext(void)
{
    QMutexLocker locker(&gl_context_lock);
    delete m_ctx;
    m_ctx = NULL;
}

bool VideoOutputOpenGLVAAPI::CreateBuffers(void)
{
    QMutexLocker locker(&gl_context_lock);
    if (codec_is_vaapi(video_codec_id))
    {
        vbuffers.Init(24, true, 2, 1, 4, 1);
        return true;
    }
    return VideoOutputOpenGL::CreateBuffers();
}

void* VideoOutputOpenGLVAAPI::GetDecoderContext(unsigned char* buf, uint8_t*& id)
{
    if (m_ctx)
    {
        id = GetSurfaceIDPointer(buf);
        return &m_ctx->m_ctx;
    }
    return NULL;
}

uint8_t* VideoOutputOpenGLVAAPI::GetSurfaceIDPointer(void* buf)
{
    if (m_ctx)
        return m_ctx->GetSurfaceIDPointer(buf);
    return NULL;
}

void VideoOutputOpenGLVAAPI::SetProfile(void)
{
    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("openglvaapi");
}

bool VideoOutputOpenGLVAAPI::ApproveDeintFilter(const QString &filtername) const
{
    return filtername.contains("vaapi");
}

bool VideoOutputOpenGLVAAPI::SetDeinterlacingEnabled(bool enable)
{
    m_deinterlacing = enable;
    SetupDeinterlace(enable);
    return m_deinterlacing;
}

bool VideoOutputOpenGLVAAPI::SetupDeinterlace(bool i, const QString& ovrf)
{
    //m_deintfiltername = !db_vdisp_profile ? "" :
    //                     db_vdisp_profile->GetFilteredDeint(ovrf);
    m_deinterlacing = i;
    return m_deinterlacing;
}

void VideoOutputOpenGLVAAPI::InitPictureAttributes(void)
{
    if (codec_is_vaapi(video_codec_id))
    {
        if (m_ctx)
            m_ctx->InitPictureAttributes(videoColourSpace);
        return;
    }
    VideoOutputOpenGL::InitPictureAttributes();
}

int VideoOutputOpenGLVAAPI::SetPictureAttribute(PictureAttribute attribute,
                                                int newValue)
{
    int val = newValue;
    if (codec_is_vaapi(video_codec_id) && m_ctx)
        val = m_ctx->SetPictureAttribute(attribute, newValue);
    return VideoOutput::SetPictureAttribute(attribute, val);
}

void VideoOutputOpenGLVAAPI::UpdatePauseFrame(int64_t &disp_timecode)
{
    if (codec_is_std(video_codec_id))
    {
        VideoOutputOpenGL::UpdatePauseFrame(disp_timecode);
        return;
    }

    vbuffers.begin_lock(kVideoBuffer_used);
    if (vbuffers.Size(kVideoBuffer_used))
    {
        VideoFrame *frame = vbuffers.Head(kVideoBuffer_used);
        CopyFrame(&av_pause_frame, frame);
        m_pauseBuffer = frame->buf;
        disp_timecode = frame->disp_timecode;
    }
    else
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            "Could not update pause frame - no used frames.");

    vbuffers.end_lock();
}

void VideoOutputOpenGLVAAPI::ProcessFrame(VideoFrame *frame, OSD *osd,
                                          FilterChain *filterList,
                                          const PIPMap &pipPlayers,
                                          FrameScanType scan)
{
    QMutexLocker locker(&gl_context_lock);
    VideoOutputOpenGL::ProcessFrame(frame, osd, filterList, pipPlayers, scan);

    if (codec_is_vaapi(video_codec_id) && m_ctx && gl_videochain)
    {
        gl_context->makeCurrent();
        m_ctx->CopySurfaceToTexture(frame ? frame->buf : m_pauseBuffer,
                                    gl_videochain->GetInputTexture(),
                                    gl_videochain->GetTextureType(), scan);
        gl_videochain->SetInputUpdated();
        gl_context->doneCurrent();
    }
}

QStringList VideoOutputOpenGLVAAPI::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;
    QStringList list;
    if ((codec_is_std(myth_codec_id) || (codec_is_vaapi(myth_codec_id))) &&
         !getenv("NO_VAAPI"))
    {
        list += "openglvaapi";
    }
    return list;
}

MythCodecID VideoOutputOpenGLVAAPI::GetBestSupportedCodec(
    uint width,       uint height, const QString &decoder,
    uint stream_type, bool no_acceleration,
    PixelFormat &pix_fmt)
{
    QSize size(width, height);
    bool use_cpu = no_acceleration;
    PixelFormat fmt = PIX_FMT_YUV420P;
    MythCodecID test_cid = (MythCodecID)(kCodec_MPEG1_VAAPI + (stream_type - 1));
    if (codec_is_vaapi(test_cid) && decoder == "vaapi" && !getenv("NO_VAAPI"))
        use_cpu |= !VAAPIContext::IsFormatAccelerated(size, test_cid, fmt);
    else
        use_cpu = true;

    if (use_cpu)
        return (MythCodecID)(kCodec_MPEG1 + (stream_type - 1));

    pix_fmt = fmt;
    return test_cid;
}
