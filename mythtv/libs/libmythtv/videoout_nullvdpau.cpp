#include "videoout_nullvdpau.h"
#define LOC QString("NullVDPAU: ")

#define MIN_REFERENCE_FRAMES 2
#define MAX_REFERENCE_FRAMES 16
#define MIN_PROCESS_BUFFER   6

void VideoOutputNullVDPAU::GetRenderOptions(render_opts &opts)
{
    opts.renderers->append("nullvdpau");
    (*opts.osds)["nullvdpau"].append("dummy");
    QStringList dummy(QString("dummy"));
    opts.deints->insert("nullvdpau", dummy);
    if (opts.decoders->contains("vdpau"))
        (*opts.safe_renderers)["vdpau"].append("nullvdpau");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("nullvdpau");
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append("nullvdpau");
    (*opts.safe_renderers)["dummy"].append("nullvdpau");
    (*opts.safe_renderers)["nuppel"].append("nullvdpau");

    opts.priorities->insert("nullvdpau", 20);
}

VideoOutputNullVDPAU::VideoOutputNullVDPAU()
  : m_render(NULL), m_lock(QMutex::Recursive), m_decoder(0), m_pix_fmt(-1),
    m_decoder_buffer_size(MAX_REFERENCE_FRAMES),
    m_checked_surface_ownership(false), m_shadowBuffers(NULL),
    m_surfaceSize(QSize(0,0))
{
}

VideoOutputNullVDPAU::~VideoOutputNullVDPAU()
{
    QMutexLocker locker(&m_lock);
    TearDown();
}

void VideoOutputNullVDPAU::TearDown(void)
{
    DeleteBuffers();
    DeleteShadowBuffers();
    DeleteRender();
}

bool VideoOutputNullVDPAU::Init(const QSize &video_dim_buf,
                                const QSize &video_dim_disp,
                                float aspect,
                                WId winid, const QRect &win_rect,
                                MythCodecID codec_id)
{
    QMutexLocker locker(&m_lock);
    bool ok = VideoOutput::Init(video_dim_buf, video_dim_disp,
                                aspect, winid, win_rect, codec_id);
    if (!codec_is_vdpau_hw(video_codec_id))
        return false;

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("nullvdpau");
    if (ok) ok = InitRender();
    if (ok) ok = InitBuffers();
    if (ok) ok = InitShadowBuffers();
    if (!ok)
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        "Created VDPAU context with GPU decoding)");
    return ok;
}

bool VideoOutputNullVDPAU::InitRender(void)
{
    QMutexLocker locker(&m_lock);
    m_render = new MythRenderVDPAU();
    if (m_render && m_render->CreateDecodeOnly())
        return true;
    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VDPAU");
    return false;
}

void VideoOutputNullVDPAU::DeleteRender(void)
{
    QMutexLocker locker(&m_lock);
    if (m_render)
    {
        if (m_decoder)
            m_render->DestroyDecoder(m_decoder);
        m_render->DecrRef();
    }

    m_decoder = 0;
    m_render = NULL;
    m_pix_fmt = -1;
}

bool VideoOutputNullVDPAU::InitBuffers(void)
{
    QMutexLocker locker(&m_lock);
    if (!m_render || !codec_is_vdpau_hw(video_codec_id))
        return false;

    uint buffer_size = m_decoder_buffer_size + MIN_PROCESS_BUFFER;
    const QSize video_dim = window.GetActualVideoDim();
    vbuffers.Init(buffer_size, false, 2, 1, 4, 1);
    bool ok = CreateVideoSurfaces(buffer_size);
    if (ok)
    {
        for (int i = 0; i < m_video_surfaces.size(); i++)
            ok &= vbuffers.CreateBuffer(video_dim.width(),
                                video_dim.height(), i,
                                m_render->GetRender(m_video_surfaces[i]),
                                FMT_VDPAU);
    }

    if (!ok)
    {
        DeleteBuffers();
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to create VDPAU buffers");
        return false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Created VDPAU buffers");
    return ok;
}

void VideoOutputNullVDPAU::DeleteBuffers(void)
{
    QMutexLocker locker(&m_lock);
    DiscardFrames(true);
    DeleteVideoSurfaces();
    vbuffers.Reset();
    vbuffers.DeleteBuffers();
    m_checked_surface_ownership = false;
}

bool VideoOutputNullVDPAU::InitShadowBuffers(void)
{
    QMutexLocker locker(&m_lock);
    if (!codec_is_vdpau_hw(video_codec_id))
        return false;

    DeleteShadowBuffers();

    uint buffer_size = m_decoder_buffer_size + MIN_PROCESS_BUFFER;
    if ((vbuffers.Size() != buffer_size) ||
        (vbuffers.Size() != (uint)m_video_surfaces.size()))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Number of GPU buffers is wrong.");
        return false;
    }

    m_surfaceSize = m_render->GetSurfaceSize(m_video_surfaces[0]);
    m_shadowBuffers = new VideoBuffers();
    if (!m_shadowBuffers)
        return false;

    m_shadowBuffers->Init(buffer_size, false, 2, 1, 4, 1);
    if (!m_shadowBuffers->CreateBuffers(FMT_YV12,
                                      m_surfaceSize.width(),
                                      m_surfaceSize.height()))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create shadow buffers.");
        DeleteShadowBuffers();
        return false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created %1 CPU buffers (%2x%3)")
        .arg(buffer_size).arg(m_surfaceSize.width())
        .arg(m_surfaceSize.height()));
    return true;
}

void VideoOutputNullVDPAU::DeleteShadowBuffers(void)
{
    QMutexLocker locker(&m_lock);
    if (!m_shadowBuffers)
        return;

    m_shadowBuffers->Reset();
    m_shadowBuffers->DeleteBuffers();
    delete m_shadowBuffers;
    m_shadowBuffers = NULL;
}

bool VideoOutputNullVDPAU::CreateVideoSurfaces(uint num)
{
    if (!m_render || num < 1)
        return false;

    bool ret = true;
    const QSize size = window.GetActualVideoDim();
    for (uint i = 0; i < num; i++)
    {
        uint tmp = m_render->CreateVideoSurface(size);
        if (tmp)
        {
            m_video_surfaces.push_back(tmp);
            m_render->ClearVideoSurface(tmp);
        }
        else
        {
            ret = false;
            break;
        }
    }
    return ret;
}

void VideoOutputNullVDPAU::DeleteVideoSurfaces(void)
{
    if (!m_render || !m_video_surfaces.size())
        return;

    for (int i = 0; i < m_video_surfaces.size(); i++)
        m_render->DestroyVideoSurface(m_video_surfaces[i]);
    m_video_surfaces.clear();
}

void VideoOutputNullVDPAU::ClaimVideoSurfaces(void)
{
    if (!m_render)
        return;

    QMutexLocker locker(&m_lock);
    QVector<uint>::iterator it;
    for (it = m_video_surfaces.begin(); it != m_video_surfaces.end(); ++it)
        m_render->ChangeVideoSurfaceOwner(*it);
    m_checked_surface_ownership = true;
}

void VideoOutputNullVDPAU::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    if (m_render && m_render->IsErrored())
        errorState = kError_Unknown;

    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("IsErrored() in DrawSlice"));
        return;
    }

    if (!codec_is_vdpau_hw(video_codec_id) || !m_render)
        return;

    if (!m_checked_surface_ownership)
        ClaimVideoSurfaces();

    struct vdpau_render_state *render = (struct vdpau_render_state *)frame->buf;
    if (!render)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No video surface to decode to.");
        errorState = kError_Unknown;
        return;
    }

    if (frame->pix_fmt != m_pix_fmt)
    {
        if (m_decoder)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Picture format has changed.");
            errorState = kError_Unknown;
            return;
        }

        uint max_refs = MIN_REFERENCE_FRAMES;
        if (frame->pix_fmt == PIX_FMT_VDPAU_H264)
        {
            max_refs = render->info.h264.num_ref_frames;
            if (max_refs < 1 || max_refs > MAX_REFERENCE_FRAMES)
            {
                uint32_t round_width  = (frame->width + 15) & ~15;
                uint32_t round_height = (frame->height + 15) & ~15;
                uint32_t surf_size    = (round_width * round_height * 3) / 2;
                max_refs = (12 * 1024 * 1024) / surf_size;
            }
            if (max_refs > MAX_REFERENCE_FRAMES)
                max_refs = MAX_REFERENCE_FRAMES;

            // Add extra buffers as necessary
            int needed = max_refs - m_decoder_buffer_size;
            if (needed > 0)
            {
                QMutexLocker locker(&m_lock);
                const QSize size = window.GetActualVideoDim();
                uint created = 0;
                for (int i = 0; i < needed; i++)
                {
                    uint tmp = m_render->CreateVideoSurface(size);
                    if (tmp)
                    {
                        m_video_surfaces.push_back(tmp);
                        m_render->ClearVideoSurface(tmp);
                        if (vbuffers.AddBuffer(size.width(), size.height(),
                                               m_render->GetRender(tmp),
                                               FMT_VDPAU))
                        {
                            created++;
                            int size = (m_surfaceSize.width() *
                                        m_surfaceSize.height() * 3) / 2;
                            m_shadowBuffers->AddBuffer(m_surfaceSize.width(),
                                                       m_surfaceSize.height(),
                                                       new unsigned char[size],
                                                       FMT_YV12);
                        }
                    }
                }
                m_decoder_buffer_size += created;
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("Added %1 new buffers. New buffer size %2 "
                            "(%3 decode and %4 process)")
                                .arg(created).arg(vbuffers.Size())
                                .arg(m_decoder_buffer_size)
                                .arg(MIN_PROCESS_BUFFER));
            }
        }

        VdpDecoderProfile vdp_decoder_profile;
        switch (frame->pix_fmt)
        {
            case PIX_FMT_VDPAU_MPEG1:
                vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG1;
                break;
            case PIX_FMT_VDPAU_MPEG2:
                vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG2_MAIN;
                break;
            case PIX_FMT_VDPAU_MPEG4:
                vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG4_PART2_ASP;
                break;
            case PIX_FMT_VDPAU_H264:
                vdp_decoder_profile = VDP_DECODER_PROFILE_H264_HIGH;
                break;
            case PIX_FMT_VDPAU_WMV3:
                vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_MAIN;
                break;
            case PIX_FMT_VDPAU_VC1:
                vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
                break;
            default:
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Picture format is not supported.");
                errorState = kError_Unknown;
                return;
        }

        m_decoder = m_render->CreateDecoder(window.GetActualVideoDim(),
                                            vdp_decoder_profile, max_refs);
        if (m_decoder)
        {
            m_pix_fmt = frame->pix_fmt;
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Created VDPAU decoder (%1 ref frames)")
                    .arg(max_refs));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create decoder.");
            errorState = kError_Unknown;
            return;
        }
    }
    else if (!m_decoder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Pix format already set but no VDPAU decoder.");
        errorState = kError_Unknown;
        return;
    }

    m_render->Decode(m_decoder, render);
}

void VideoOutputNullVDPAU::ClearAfterSeek(void)
{
    QMutexLocker locker(&m_lock);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "ClearAfterSeek()");
    DiscardFrames(false);
}

// Always returns the CPU version of a frame
VideoFrame* VideoOutputNullVDPAU::GetLastDecodedFrame(void)
{
    if (!BufferSizeCheck())
        return NULL;

    VideoFrame* gpu = vbuffers.GetLastDecodedFrame();
    for (uint i = 0; i < vbuffers.Size(); i++)
        if (vbuffers.At(i) == gpu)
            return m_shadowBuffers->At(i);
    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find frame.");
    return NULL;
}

// Always returns the CPU version of a frame
VideoFrame* VideoOutputNullVDPAU::GetLastShownFrame(void)
{
    if (!BufferSizeCheck())
        return NULL;

    VideoFrame* gpu = vbuffers.GetLastShownFrame();
    for (uint i = 0; i < vbuffers.Size(); i++)
        if (vbuffers.At(i) == gpu)
            return m_shadowBuffers->At(i);
    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find frame.");
    return NULL;
}

// Should work with either the CPU or GPU version of a frame
void VideoOutputNullVDPAU::DiscardFrame(VideoFrame *frame)
{
    if (!frame || !BufferSizeCheck())
        return;

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
            vbuffers.DoneDisplayingFrame(frame);
            m_lock.unlock();
            return;
        }
    }
}

// Should work with either the CPU or GPU version of a frame
void VideoOutputNullVDPAU::DoneDisplayingFrame(VideoFrame *frame)
{
    if (!frame || !BufferSizeCheck())
        return;

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
            if (vbuffers.Contains(kVideoBuffer_used, frame))
                DiscardFrame(frame);
            CheckFrameStates();
            m_lock.unlock();
            return;
        }
    }
}

void VideoOutputNullVDPAU::ReleaseFrame(VideoFrame *frame)
{
    if (!frame)
        return;

    if ((frame->codec == FMT_VDPAU) && m_render)
    {
        if (BufferSizeCheck())
        {
            uint surface = 0;
            struct vdpau_render_state *render =
                    (struct vdpau_render_state *)frame->buf;
            if (render)
                surface = m_render->GetSurfaceOwner(render->surface);
            // assume a direct mapping of GPU to CPU buffers
            for (uint i = 0; i < vbuffers.Size(); i++)
            {
                if (vbuffers.At(i)->buf == frame->buf)
                {
                    VideoFrame *vf = m_shadowBuffers->At(i);
                    uint32_t pitches[] = {
                        (uint32_t)vf->pitches[0],
                        (uint32_t)vf->pitches[2],
                        (uint32_t)vf->pitches[1] };
                    void* const planes[] = {
                        vf->buf,
                        vf->buf + vf->offsets[2],
                        vf->buf + vf->offsets[1] };
                    if (!m_render->DownloadYUVFrame(surface, planes, pitches))
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC +
                            "Failed to get frame from GPU.");
                    }
                    vf->aspect = frame->aspect;
                    vf->disp_timecode = frame->disp_timecode;
                    vf->dummy = frame->dummy;
                    vf->frameNumber = frame->frameNumber;
                    vf->interlaced_frame = frame->interlaced_frame;
                    vf->timecode = frame->timecode;
                    vf->repeat_pict = frame->repeat_pict;
                    vf->top_field_first = frame->top_field_first;
                }
            }
        }
    }

    VideoOutput::ReleaseFrame(frame);
}

bool VideoOutputNullVDPAU::InputChanged(const QSize &video_dim_buf,
                                        const QSize &video_dim_disp,
                                        float        aspect,
                                        MythCodecID  av_codec_id,
                                        void        *codec_private,
                                        bool        &aspect_only)
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

QStringList VideoOutputNullVDPAU::GetAllowedRenderers(MythCodecID myth_codec_id)
{
    QStringList list;
    if (codec_is_vdpau_hw(myth_codec_id) && !getenv("NO_VDPAU"))
        list += "nullvdpau";
    return list;
}

void VideoOutputNullVDPAU::DiscardFrames(bool next_frame_keyframe)
{
    QMutexLocker locker(&m_lock);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DiscardFrames(%1)")
            .arg(next_frame_keyframe));
    CheckFrameStates();
    vbuffers.DiscardFrames(next_frame_keyframe);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DiscardFrames() 3: %1 -- done()")
            .arg(vbuffers.GetStatus()));
}

void VideoOutputNullVDPAU::CheckFrameStates(void)
{
    QMutexLocker locker(&m_lock);
    frame_queue_t::iterator it;
    it = vbuffers.begin_lock(kVideoBuffer_displayed);
    while (it != vbuffers.end(kVideoBuffer_displayed))
    {
        VideoFrame* frame = *it;
        if (vbuffers.Contains(kVideoBuffer_decode, frame))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Frame %1 is in use by avlib and so is "
                        "being held for later discarding.")
                        .arg(DebugString(frame, true)));
        }
        else
        {
            vbuffers.SafeEnqueue(kVideoBuffer_avail, frame);
            vbuffers.end_lock();
            it = vbuffers.begin_lock(kVideoBuffer_displayed);
            continue;
        }
        ++it;
    }
    vbuffers.end_lock();
}

bool VideoOutputNullVDPAU::BufferSizeCheck(void)
{
    if (vbuffers.Size() != m_shadowBuffers->Size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Number of GPU buffers not the "
                                       "same as number of CPU buffers.");
        return false;
    }
    return true;
}
