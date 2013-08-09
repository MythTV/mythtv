#include "mythcontext.h"
#include "mythplayer.h"
#include "videooutbase.h"
#include "videoout_vdpau.h"
#include "videodisplayprofile.h"
#include "osd.h"
#include "mythxdisplay.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythpainter_vdpau.h"

#define LOC      QString("VidOutVDPAU: ")

#define MIN_REFERENCE_FRAMES 2
#define MAX_REFERENCE_FRAMES 16
#define MIN_PROCESS_BUFFER   6
#define MAX_PROCESS_BUFFER   50
#define DEF_PROCESS_BUFFER   12

#define CHECK_ERROR(Loc) \
  if (m_render && m_render->IsErrored()) \
      errorState = kError_Unknown; \
  if (IsErrored()) \
  { \
      LOG(VB_GENERAL, LOG_ERR, LOC + QString("IsErrored() in %1").arg(Loc)); \
      return; \
  } while(0)

void VideoOutputVDPAU::GetRenderOptions(render_opts &opts)
{
    opts.renderers->append("vdpau");
    (*opts.osds)["vdpau"].append("vdpau");
    if (opts.decoders->contains("vdpau"))
        (*opts.safe_renderers)["vdpau"].append("vdpau");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("vdpau");
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append("vdpau");
    (*opts.safe_renderers)["dummy"].append("vdpau");
    (*opts.safe_renderers)["nuppel"].append("vdpau");

    opts.priorities->insert("vdpau", 120);
    QStringList deints;
    deints += "none";
    deints += "vdpauonefield";
    deints += "vdpaubobdeint";
    deints += "vdpaubasic";
    deints += "vdpauadvanced";
    deints += "vdpaubasicdoublerate";
    deints += "vdpauadvanceddoublerate";
    opts.deints->insert("vdpau", deints);
}

VideoOutputVDPAU::VideoOutputVDPAU()
  : m_win(0),                m_render(NULL),
    m_decoder_buffer_size(MAX_REFERENCE_FRAMES),
    m_process_buffer_size(DEF_PROCESS_BUFFER), m_pause_surface(0),
    m_need_deintrefs(false), m_video_mixer(0), m_mixer_features(kVDPFeatNone),
    m_checked_surface_ownership(false),
    m_checked_output_surfaces(false),
    m_decoder(0),            m_pix_fmt(-1),
    m_lock(QMutex::Recursive), m_pip_layer(0), m_pip_surface(0),
    m_pip_ready(false),      m_osd_painter(NULL),
    m_skip_chroma(false),    m_denoise(0.0f),
    m_sharpen(0.0f),
    m_colorspace(VDP_COLOR_STANDARD_ITUR_BT_601)
{
    if (gCoreContext->GetNumSetting("UseVideoModes", 0))
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
    DeinitPIPS();
    DeinitPIPLayer();
    DeleteBuffers();
    RestoreDisplay();
    DeleteRender();
}

bool VideoOutputVDPAU::Init(const QSize &video_dim_buf,
                            const QSize &video_dim_disp,
                            float aspect,
                            WId winid, const QRect &win_rect,
                            MythCodecID codec_id)
{
    // Attempt to free up as much video memory as possible
    // only works when using the VDPAU painter for the UI
    MythPainter *painter = GetMythPainter();
    if (painter)
        painter->FreeResources();

    m_win = winid;
    QMutexLocker locker(&m_lock);
    window.SetNeedRepaint(true);
    bool ok = VideoOutput::Init(video_dim_buf, video_dim_disp,
                                aspect, winid, win_rect,codec_id);
    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("vdpau");

    InitDisplayMeasurements(video_dim_disp.width(), video_dim_disp.height(),
                            true);
    ParseOptions();
    if (ok) ok = InitRender();
    if (ok) ok = InitBuffers();
    if (!ok)
    {
        TearDown();
        return ok;
    }

    InitPictureAttributes();
    MoveResize();
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Created VDPAU context (%1 decode)")
            .arg(codec_is_std(video_codec_id) ? "software" : "GPU"));

    return ok;
}

bool VideoOutputVDPAU::InitRender(void)
{
    QMutexLocker locker(&m_lock);

    const QSize size = window.GetDisplayVisibleRect().size();
    m_render = new MythRenderVDPAU();

    if (m_render && m_render->Create(size, m_win))
    {
        m_osd_painter = new MythVDPAUPainter(m_render);
        if (m_osd_painter)
        {
            m_osd_painter->SetSwapControl(false);
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Created VDPAU osd (%1x%2)")
                    .arg(size.width()).arg(size.height()));
        }
        else
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VDPAU osd.");
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VDPAU");

    return false;
}

void VideoOutputVDPAU::DeleteRender(void)
{
    QMutexLocker locker(&m_lock);

    if (m_osd_painter)
        delete m_osd_painter;

    if (m_render)
    {
        if (m_decoder)
            m_render->DestroyDecoder(m_decoder);

        m_render->DecrRef();
        m_render = NULL;
    }

    m_checked_output_surfaces = false;
    m_osd_painter = NULL;
    m_decoder = 0;
    m_render = NULL;
    m_pix_fmt = -1;
}

bool VideoOutputVDPAU::InitBuffers(void)
{
    QMutexLocker locker(&m_lock);
    if (!m_render)
        return false;

    uint buffer_size = m_decoder_buffer_size + m_process_buffer_size;
    const QSize video_dim = codec_is_std(video_codec_id) ?
                            window.GetVideoDim() : window.GetActualVideoDim();

    vbuffers.Init(buffer_size, false, 2, 1, 4, 1);

    bool ok = false;
    if (codec_is_vdpau(video_codec_id))
    {
        ok = CreateVideoSurfaces(buffer_size);
        if (ok)
        {
            for (int i = 0; i < m_video_surfaces.size(); i++)
                ok &= vbuffers.CreateBuffer(video_dim.width(),
                                    video_dim.height(), i,
                                    m_render->GetRender(m_video_surfaces[i]),
                                    FMT_VDPAU);
        }
    }
    else if (codec_is_std(video_codec_id))
    {
        ok = CreateVideoSurfaces(NUM_REFERENCE_FRAMES);
        if (ok)
            ok = vbuffers.CreateBuffers(FMT_YV12,
                                        video_dim.width(), video_dim.height());
    }

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to create VDPAU buffers");
    }
    else
    {
        m_video_mixer = m_render->CreateVideoMixer(video_dim, 2,
                                                   m_mixer_features);
        ok = m_video_mixer;
        m_pause_surface = m_video_surfaces[0];

        if (ok && (m_mixer_features & kVDPFeatSharpness))
            m_render->SetMixerAttribute(m_video_mixer,
                                        kVDPAttribSharpness,
                                        m_sharpen);
        if (ok && (m_mixer_features & kVDPFeatDenoise))
            m_render->SetMixerAttribute(m_video_mixer,
                                        kVDPAttribNoiseReduction,
                                        m_denoise);
        if (ok && m_skip_chroma)
            m_render->SetMixerAttribute(m_video_mixer,
                                        kVDPAttribSkipChroma, 1);

        if (ok && (db_letterbox_colour == kLetterBoxColour_Gray25))
            m_render->SetMixerAttribute(m_video_mixer,
                                        kVDPAttribBackground, 0x7F7F7FFF);
    }

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to create VDPAU mixer");
        DeleteBuffers();
    }

    return ok;
}

bool VideoOutputVDPAU::CreateVideoSurfaces(uint num)
{
    if (!m_render || num < 1)
        return false;

    bool ret = true;
    const QSize size = codec_is_std(video_codec_id) ?
                       window.GetVideoDim() : window.GetActualVideoDim();
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

void VideoOutputVDPAU::DeleteVideoSurfaces(void)
{
    if (!m_render || m_video_surfaces.isEmpty())
        return;

    for (int i = 0; i < m_video_surfaces.size(); i++)
        m_render->DestroyVideoSurface(m_video_surfaces[i]);
    m_video_surfaces.clear();
}

void VideoOutputVDPAU::DeleteBuffers(void)
{
    QMutexLocker locker(&m_lock);
    if (m_render && m_video_mixer)
        m_render->DestroyVideoMixer(m_video_mixer);
    m_video_mixer = 0;
    m_checked_surface_ownership = false;
    DiscardFrames(true);
    DeleteVideoSurfaces();
    vbuffers.Reset();
    vbuffers.DeleteBuffers();
}

void VideoOutputVDPAU::RestoreDisplay(void)
{
    QMutexLocker locker(&m_lock);

    const QRect tmp_display_visible_rect =
        window.GetTmpDisplayVisibleRect();
    if (window.GetPIPState() == kPIPStandAlone &&
        !tmp_display_visible_rect.isEmpty())
    {
        window.SetDisplayVisibleRect(tmp_display_visible_rect);
    }
    const QRect display_visible_rect = window.GetDisplayVisibleRect();

    if (m_render)
        m_render->DrawDisplayRect(display_visible_rect);
}

bool VideoOutputVDPAU::SetDeinterlacingEnabled(bool interlaced)
{
    if ((interlaced && m_deinterlacing) ||
       (!interlaced && !m_deinterlacing))
        return m_deinterlacing;

    return SetupDeinterlace(interlaced);
}

bool VideoOutputVDPAU::SetupDeinterlace(bool interlaced,
                                        const QString &override)
{
    m_lock.lock();
    if (!m_render)
        return false;

    bool enable = interlaced;
    if (enable)
    {
        m_deintfiltername = db_vdisp_profile->GetFilteredDeint(override);
        if (m_deintfiltername.contains("vdpau"))
        {
            uint features = kVDPFeatNone;
            bool spatial  = m_deintfiltername.contains("advanced");
            bool temporal = m_deintfiltername.contains("basic") || spatial;
            m_need_deintrefs = spatial || temporal;

            if (temporal)
                features += kVDPFeatTemporal;

            if (spatial)
                features += kVDPFeatSpatial;

            enable = m_render->SetDeinterlacing(m_video_mixer, features);
            if (enable)
            {
                m_deinterlacing = true;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Enabled deinterlacing.");
            }
            else
            {
                enable = false;
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    "Failed to enable deinterlacing.");
            }
        }
        else
        {
            enable = false;
        }
    }

    if (!enable)
    {
        ClearReferenceFrames();
        m_render->SetDeinterlacing(m_video_mixer);
        m_deintfiltername = QString();
        m_deinterlacing   = false;
        m_need_deintrefs  = false;
    }
    m_lock.unlock();
    return enable;
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
    QMutexLocker locker(&m_lock);
    CHECK_ERROR("ProcessFrame");

    if (!m_checked_surface_ownership && codec_is_std(video_codec_id))
        ClaimVideoSurfaces();

    m_pip_ready = false;
    ShowPIPs(frame, pipPlayers);
}

void VideoOutputVDPAU::ClearDummyFrame(VideoFrame *frame)
{
    if (frame && m_render && !codec_is_std(video_codec_id))
    {
        struct vdpau_render_state *render =
            (struct vdpau_render_state *)frame->buf;
        if (render)
            m_render->ClearVideoSurface(render->surface);
    }
    VideoOutput::ClearDummyFrame(frame);
}

void VideoOutputVDPAU::PrepareFrame(VideoFrame *frame, FrameScanType scan,
                                    OSD *osd)
{
    QMutexLocker locker(&m_lock);
    (void)osd;
    CHECK_ERROR("PrepareFrame");

    if (!m_render)
        return;

    if (!m_checked_output_surfaces &&
        !(!codec_is_std(video_codec_id) && !m_decoder))
    {
        m_render->CheckOutputSurfaces();
        m_checked_output_surfaces = true;
    }

    bool new_frame = false;
    bool dummy     = false;
    if (frame)
    {
        // FIXME for 0.23. This should be triggered from AFD by a seek
        if ((abs(frame->frameNumber - framesPlayed) > 8))
            ClearReferenceFrames();
        new_frame = (framesPlayed != frame->frameNumber + 1);
        framesPlayed = frame->frameNumber + 1;
        dummy = frame->dummy;
    }

    uint video_surface = m_video_surfaces[0];
    bool deint = (m_deinterlacing && m_need_deintrefs &&
                  frame && !dummy);

    if (deint)
    {
        if (new_frame)
            UpdateReferenceFrames(frame);
        if (m_reference_frames.size() != NUM_REFERENCE_FRAMES)
            deint = false;
    }

    if (!codec_is_std(video_codec_id) && frame)
    {
        struct vdpau_render_state *render =
            (struct vdpau_render_state *)frame->buf;
        if (!render)
            return;
        video_surface = m_render->GetSurfaceOwner(render->surface);
    }
    else if (new_frame && frame && !dummy)
    {
        // FIXME - reference frames for software decode
        if (deint)
            video_surface = m_video_surfaces[(framesPlayed + 1) %
                            NUM_REFERENCE_FRAMES];

        uint32_t pitches[3] = {
            frame->pitches[0],
            frame->pitches[2],
            frame->pitches[1]
        };
        void* const planes[3] = {
            frame->buf,
            frame->buf + frame->offsets[2],
            frame->buf + frame->offsets[1]
        };

        if (!m_render->UploadYUVFrame(video_surface, planes, pitches))
            return;
    }
    else if (!frame)
    {
        deint = false;
        video_surface = m_pause_surface;
    }

    VdpVideoMixerPictureStructure field =
        VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;

    if (scan == kScan_Interlaced && m_deinterlacing && frame)
    {
        field = frame->top_field_first ?
                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD :
                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD;
    }
    else if (scan == kScan_Intr2ndField && m_deinterlacing && frame)
    {
        field = frame->top_field_first ?
                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD :
                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    }
    else if (!frame && m_deinterlacing)
    {
        field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    }

    m_render->WaitForFlip();

    QSize size = window.GetDisplayVisibleRect().size();
    if (size != m_render->GetSize())
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unexpected display size.");

    if (dummy)
    {
        m_render->DrawBitmap(0, 0, NULL, NULL, kVDPBlendNormal, 255);
    }
    else
    {
        if (!m_render->MixAndRend(m_video_mixer, field, video_surface, 0,
                                  deint ? &m_reference_frames : NULL,
                                  scan == kScan_Interlaced,
                                  window.GetVideoRect(),
                                  QRect(QPoint(0,0), size),
                                  vsz_enabled ? vsz_desired_display_rect :
                                                window.GetDisplayVideoRect(),
                                  0, 0))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "Prepare frame failed.");
        }
    }

    if (m_pip_ready)
        m_render->DrawLayer(m_pip_layer, 0);
    if (m_visual)
        m_visual->Draw(GetTotalOSDBounds(), m_osd_painter, NULL);

    if (osd && m_osd_painter && !window.IsEmbedding())
        osd->DrawDirect(m_osd_painter, GetTotalOSDBounds().size(), true);

    if (!frame)
    {
        VideoFrame *buf = GetLastShownFrame();
        if (buf)
            buf->timecode = 0;
    }
}

void VideoOutputVDPAU::ClaimVideoSurfaces(void)
{
    if (!m_render)
        return;

    QVector<uint>::iterator it;
    for (it = m_video_surfaces.begin(); it != m_video_surfaces.end(); ++it)
        m_render->ChangeVideoSurfaceOwner(*it);
    m_checked_surface_ownership = true;
}

void VideoOutputVDPAU::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    CHECK_ERROR("DrawSlice");

    if (codec_is_std(video_codec_id) || !m_render)
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
                        }
                    }
                }
                m_decoder_buffer_size += created;
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("Added %1 new buffers. New buffer size %2 "
                            "(%3 decode and %4 process)")
                                .arg(created).arg(vbuffers.Size())
                                .arg(m_decoder_buffer_size)
                                .arg(m_process_buffer_size));
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

void VideoOutputVDPAU::Show(FrameScanType scan)
{
    QMutexLocker locker(&m_lock);
    CHECK_ERROR("Show");

    if (window.IsRepaintNeeded())
        DrawUnusedRects(false);

    if (m_render)
        m_render->Flip();
    CheckFrameStates();
}

void VideoOutputVDPAU::ClearAfterSeek(void)
{
    m_lock.lock();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "ClearAfterSeek()");
    DiscardFrames(false);
    m_lock.unlock();
}

bool VideoOutputVDPAU::InputChanged(const QSize &video_dim_buf,
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

    // Ensure we don't lose embedding through program changes. This duplicates
    // code in VideoOutput::Init but we need start here otherwise the embedding
    // is lost during window re-initialistion.
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
        aspect_only = true;
        if (asp_changed)
        {
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        if (wasembedding)
            EmbedInWidget(oldrect);
        return true;
    }

    AdjustFillMode oldadjustfillmode = window.GetAdjustFill();

    TearDown();
    QRect disp = window.GetDisplayVisibleRect();
    if (Init(video_dim_buf, video_dim_disp,
             aspect, m_win, disp, av_codec_id))
    {
        if (wasembedding)
            EmbedInWidget(oldrect);
        BestDeint();
        window.ToggleAdjustFill(oldadjustfillmode);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to re-initialise video output.");
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

void VideoOutputVDPAU::EmbedInWidget(const QRect &rect)
{
    QMutexLocker locker(&m_lock);
    if (!window.IsEmbedding())
    {
        VideoOutput::EmbedInWidget(rect);
        MoveResize();
        window.SetDisplayVisibleRect(window.GetTmpDisplayVisibleRect());
    }
}

void VideoOutputVDPAU::StopEmbedding(void)
{
    if (!window.IsEmbedding())
        return;
    QMutexLocker locker(&m_lock);
    VideoOutput::StopEmbedding();
    MoveResize();
}

void VideoOutputVDPAU::MoveResizeWindow(QRect new_rect)
{
    m_lock.lock();
    if (m_render)
        m_render->MoveResizeWin(new_rect);
    m_lock.unlock();
}

void VideoOutputVDPAU::DrawUnusedRects(bool sync)
{
    m_lock.lock();
    if (window.IsRepaintNeeded() && m_render)
    {
        const QRect dvr = window.GetDisplayVisibleRect();
        m_render->DrawDisplayRect(dvr, true);
        window.SetNeedRepaint(false);
        if (sync)
            m_render->SyncDisplay();
    }
    m_lock.unlock();
}

void VideoOutputVDPAU::UpdatePauseFrame(int64_t &disp_timecode)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "UpdatePauseFrame() " +
            vbuffers.GetStatus());

    vbuffers.begin_lock(kVideoBuffer_used);

    if (vbuffers.Size(kVideoBuffer_used) && m_render)
    {
        VideoFrame *frame = vbuffers.Head(kVideoBuffer_used);
        disp_timecode = frame->disp_timecode;
        if (codec_is_std(video_codec_id))
        {
            m_pause_surface = m_video_surfaces[0];
            uint32_t pitches[3] = { frame->pitches[0],
                                    frame->pitches[2],
                                    frame->pitches[1] };
            void* const planes[3] = { frame->buf,
                                      frame->buf + frame->offsets[2],
                                      frame->buf + frame->offsets[1] };
            m_render->UploadYUVFrame(m_video_surfaces[0], planes, pitches);
        }
        else
        {
            struct vdpau_render_state *render =
                    (struct vdpau_render_state *)frame->buf;
            if (render)
                m_pause_surface = m_render->GetSurfaceOwner(render->surface);
        }
    }
    else
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            "Could not update pause frame - no used frames.");

    vbuffers.end_lock();
}

void VideoOutputVDPAU::InitPictureAttributes(void)
{
    videoColourSpace.SetSupportedAttributes((PictureAttributeSupported)
                                     (kPictureAttributeSupported_Brightness |
                                      kPictureAttributeSupported_Contrast |
                                      kPictureAttributeSupported_Colour |
                                      kPictureAttributeSupported_Hue |
                                      kPictureAttributeSupported_StudioLevels));

    m_lock.lock();
    if (m_render && m_video_mixer)
    {
        if (m_colorspace < 0)
        {
            QSize size = window.GetVideoDim();
            m_colorspace = (size.width() > 720 || size.height() > 576) ?
                            VDP_COLOR_STANDARD_ITUR_BT_709 :
                            VDP_COLOR_STANDARD_ITUR_BT_601;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using ITU %1 colorspace")
                        .arg((m_colorspace == VDP_COLOR_STANDARD_ITUR_BT_601) ?
                        "BT.601" : "BT.709"));
        }

        if (m_colorspace != VDP_COLOR_STANDARD_ITUR_BT_601)
        {
            videoColourSpace.SetColourSpace((m_colorspace == VDP_COLOR_STANDARD_ITUR_BT_601)
                         ? kCSTD_ITUR_BT_601 : kCSTD_ITUR_BT_709);
        }
        m_render->SetCSCMatrix(m_video_mixer, videoColourSpace.GetMatrix());
    }
    m_lock.unlock();
}

int VideoOutputVDPAU::SetPictureAttribute(PictureAttribute attribute,
                                          int newValue)
{
    if (!m_render || !m_video_mixer)
        return -1;

    m_lock.lock();
    newValue = videoColourSpace.SetPictureAttribute(attribute, newValue);
    if (newValue >= 0)
        m_render->SetCSCMatrix(m_video_mixer, videoColourSpace.GetMatrix());
    m_lock.unlock();
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
    uint width, uint height, const QString &decoder,
    uint stream_type, bool no_acceleration)
{
    bool use_cpu = no_acceleration;

    MythCodecID test_cid = (MythCodecID)(kCodec_MPEG1_VDPAU + (stream_type-1));
    use_cpu |= !codec_is_vdpau_hw(test_cid);
    if (test_cid == kCodec_MPEG4_VDPAU)
        use_cpu |= !MythRenderVDPAU::IsMPEG4Available();
    if (test_cid == kCodec_H264_VDPAU)
        use_cpu |= !MythRenderVDPAU::H264DecoderSizeSupported(width, height);
    if ((decoder != "vdpau") || getenv("NO_VDPAU") || use_cpu)
        return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));

    return test_cid;
}

void VideoOutputVDPAU::UpdateReferenceFrames(VideoFrame *frame)
{
    while (m_reference_frames.size() > (NUM_REFERENCE_FRAMES - 1))
        m_reference_frames.pop_front();

    uint ref = m_video_surfaces[(framesPlayed +1)  % NUM_REFERENCE_FRAMES];
    if (!codec_is_std(video_codec_id))
    {
        struct vdpau_render_state *render =
            (struct vdpau_render_state *)frame->buf;
        if (render)
            ref = m_render->GetSurfaceOwner(render->surface);
    }

    m_reference_frames.push_back(ref);
}

bool VideoOutputVDPAU::FrameIsInUse(VideoFrame *frame)
{
    if (!frame || codec_is_std(video_codec_id))
        return false;

    uint ref = 0;
    struct vdpau_render_state *render = (struct vdpau_render_state *)frame->buf;
    if (render)
        ref = m_render->GetSurfaceOwner(render->surface);
    return m_reference_frames.contains(ref);
}

void VideoOutputVDPAU::ClearReferenceFrames(void)
{
    m_lock.lock();
    m_reference_frames.clear();
    m_lock.unlock();
}

void VideoOutputVDPAU::DiscardFrame(VideoFrame *frame)
{
    if (!frame)
        return;

    m_lock.lock();
    if (FrameIsInUse(frame))
        vbuffers.SafeEnqueue(kVideoBuffer_displayed, frame);
    else
    {
        vbuffers.DoneDisplayingFrame(frame);
    }
    m_lock.unlock();
}

void VideoOutputVDPAU::DiscardFrames(bool next_frame_keyframe)
{
    m_lock.lock();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DiscardFrames(%1)")
            .arg(next_frame_keyframe));
    CheckFrameStates();
    ClearReferenceFrames();
    vbuffers.DiscardFrames(next_frame_keyframe);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DiscardFrames() 3: %1 -- done()")
            .arg(vbuffers.GetStatus()));
    m_lock.unlock();
}

void VideoOutputVDPAU::DoneDisplayingFrame(VideoFrame *frame)
{
    m_lock.lock();
    if (vbuffers.Contains(kVideoBuffer_used, frame))
        DiscardFrame(frame);
    CheckFrameStates();
    m_lock.unlock();
}

void VideoOutputVDPAU::CheckFrameStates(void)
{
    m_lock.lock();
    frame_queue_t::iterator it;
    it = vbuffers.begin_lock(kVideoBuffer_displayed);
    while (it != vbuffers.end(kVideoBuffer_displayed))
    {
        VideoFrame* frame = *it;
        if (!FrameIsInUse(frame))
        {
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
        }
        ++it;
    }
    vbuffers.end_lock();
    m_lock.unlock();
}

bool VideoOutputVDPAU::InitPIPLayer(QSize size)
{
    if (!m_render)
        return false;

    if (!m_pip_surface)
        m_pip_surface = m_render->CreateOutputSurface(size);

    if (!m_pip_layer && m_pip_surface)
        m_pip_layer = m_render->CreateLayer(m_pip_surface);

    return (m_pip_surface && m_pip_layer);
}

void VideoOutputVDPAU::DeinitPIPS(void)
{
    while (!m_pips.empty())
    {
        RemovePIP(m_pips.begin().key());
        m_pips.erase(m_pips.begin());
    }

    m_pip_ready = false;
}

void VideoOutputVDPAU::DeinitPIPLayer(void)
{
    if (m_render)
    {
        if (m_pip_surface)
        {
            m_render->DestroyOutputSurface(m_pip_surface);
            m_pip_surface = 0;
        }

        if (m_pip_layer)
        {
            m_render->DestroyLayer(m_pip_layer);
            m_pip_layer = 0;
        }
    }

    m_pip_ready = false;
}

void VideoOutputVDPAU::ShowPIP(VideoFrame *frame, MythPlayer *pipplayer,
                               PIPLocation loc)
{
    (void) frame;
    if (!pipplayer || !m_render)
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

    if (InitPIPLayer(window.GetDisplayVisibleRect().size()))
    {
        if (m_pips.contains(pipplayer) &&
            m_pips[pipplayer].videoSize != pipVideoDim)
            RemovePIP(pipplayer);

        if (!m_pips.contains(pipplayer))
        {
            uint mixer = m_render->CreateVideoMixer(pipVideoDim, 0, 0);
            uint surf  = m_render->CreateVideoSurface(pipVideoDim);
            vdpauPIP tmp = { pipVideoDim, surf, mixer};
            m_pips.insert(pipplayer, tmp);
            if (!mixer || !surf)
                RemovePIP(pipplayer);
            else
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created pip %1x%2")
                    .arg(pipVideoDim.width()).arg(pipVideoDim.height()));
        }

        if (m_pips.contains(pipplayer))
        {
            QRect rect = GetPIPRect(loc, pipplayer);

            if (!m_pip_ready)
                m_render->DrawBitmap(0, m_pip_surface, NULL, NULL,
                                     kVDPBlendNull);

            uint32_t pitches[] = {
                pipimage->pitches[0],
                pipimage->pitches[2],
                pipimage->pitches[1] };
            void* const planes[] = {
                pipimage->buf,
                pipimage->buf + pipimage->offsets[2],
                pipimage->buf + pipimage->offsets[1] };

            bool ok;
            ok = m_render->UploadYUVFrame(m_pips[pipplayer].videoSurface,
                                          planes, pitches);
            ok &= m_render->MixAndRend(m_pips[pipplayer].videoMixer,
                                       VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
                                       m_pips[pipplayer].videoSurface,
                                       m_pip_surface, NULL, false,
                                       QRect(QPoint(0,0), pipVideoDim),
                                       rect, rect);
            ok &= m_render->DrawBitmap(0, m_pip_surface, NULL, &rect,
                                       kVDPBlendPiP, 255);

            if (pipActive)
            {
                // TODO this could be one rect rendered before the video frame
                QRect l = QRect(QPoint(rect.x() - 10, rect.y() - 10),
                                QSize(10, rect.height() + 20));
                QRect t = QRect(QPoint(rect.x(), rect.y() - 10),
                                QSize(rect.width(), 10));
                QRect b = QRect(QPoint(rect.x(), rect.y() + rect.height()),
                                QSize(rect.width(), 10));
                QRect r = QRect(QPoint(rect.x() + rect.width(), rect.y() -10),
                                QSize(10, rect.height() + 20));
                m_render->DrawBitmap(0, m_pip_surface, NULL, &l, kVDPBlendNormal, 255, 127);
                m_render->DrawBitmap(0, m_pip_surface, NULL, &t, kVDPBlendNormal, 255, 127);
                m_render->DrawBitmap(0, m_pip_surface, NULL, &b, kVDPBlendNormal, 255, 127);
                m_render->DrawBitmap(0, m_pip_surface, NULL, &r, kVDPBlendNormal, 255, 127);
            }

            m_pip_ready = ok;
        }
    }
    pipplayer->ReleaseCurrentFrame(pipimage);
}

void VideoOutputVDPAU::RemovePIP(MythPlayer *pipplayer)
{
    if (!m_pips.contains(pipplayer))
        return;

    if (m_pips[pipplayer].videoSurface && m_render)
        m_render->DestroyVideoSurface(m_pips[pipplayer].videoSurface);

    if (m_pips[pipplayer].videoMixer && m_render)
        m_render->DestroyVideoMixer(m_pips[pipplayer].videoMixer);

    m_pips.remove(pipplayer);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Removed 1 PIP");

    if (m_pips.empty())
        DeinitPIPLayer();
}

void VideoOutputVDPAU::ParseOptions(void)
{
    m_skip_chroma = false;
    m_denoise     = 0.0f;
    m_sharpen     = 0.0f;
    m_colorspace  = VDP_COLOR_STANDARD_ITUR_BT_601;
    m_mixer_features = kVDPFeatNone;

    m_decoder_buffer_size = MAX_REFERENCE_FRAMES;
    m_process_buffer_size = DEF_PROCESS_BUFFER;
    if (codec_is_vdpau(video_codec_id))
        m_decoder_buffer_size = MIN_REFERENCE_FRAMES;

    QStringList list = GetFilters().split(",");
    if (list.empty())
        return;

    for (QStringList::Iterator i = list.begin(); i != list.end(); ++i)
    {
        QString name = (*i).section('=', 0, 0).toLower();
        QString opts = (*i).section('=', 1).toLower();

        if (!name.contains("vdpau"))
            continue;

        if (name.contains("vdpaubuffercount"))
        {
            uint num = opts.toUInt();
            if (MIN_PROCESS_BUFFER <= num && num <= MAX_PROCESS_BUFFER)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("VDPAU process buffer size set to %1 (was %2)")
                        .arg(num).arg(m_process_buffer_size));
                m_process_buffer_size = num;
            }
        }
        else if (name.contains("vdpauivtc"))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Enabling VDPAU inverse telecine "
                "(requires Basic or Advanced deinterlacer)");
            m_mixer_features |= kVDPFeatIVTC;
        }
        else if (name.contains("vdpauskipchroma"))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Enabling SkipChromaDeinterlace.");
            m_skip_chroma = true;
        }
        else if (name.contains("vdpaudenoise"))
        {
            float tmp = std::max(0.0f, std::min(1.0f, opts.toFloat()));
            if (tmp != 0.0)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("VDPAU Denoise %1").arg(tmp,4,'f',2,'0'));
                m_denoise = tmp;
                m_mixer_features |= kVDPFeatDenoise;
            }
        }
        else if (name.contains("vdpausharpen"))
        {
            float tmp = std::max(-1.0f, std::min(1.0f, opts.toFloat()));
            if (tmp != 0.0)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("VDPAU Sharpen %1").arg(tmp,4,'f',2,'0'));
                m_sharpen = tmp;
                m_mixer_features |= kVDPFeatSharpness;
            }
        }
        else if (name.contains("vdpaucolorspace"))
        {
            if (opts.contains("auto"))
                m_colorspace = -1;
            else if (opts.contains("601"))
                m_colorspace = VDP_COLOR_STANDARD_ITUR_BT_601;
            else if (opts.contains("709"))
                m_colorspace = VDP_COLOR_STANDARD_ITUR_BT_709;

            if (m_colorspace > -1)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Forcing ITU BT.%1 colorspace")
                        .arg((m_colorspace == VDP_COLOR_STANDARD_ITUR_BT_601) ?
                             "BT.601" : "BT.709"));
            }
        }
        else if (name.contains("vdpauhqscaling"))
        {
            m_mixer_features |= kVDPFeatHQScaling;
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Requesting high quality scaling.");
        }
    }
}

MythPainter *VideoOutputVDPAU::GetOSDPainter(void)
{
    return m_osd_painter;
}

bool VideoOutputVDPAU::GetScreenShot(int width, int height, QString filename)
{
    if (m_render)
        return m_render->GetScreenShot(width, height, filename);
    return false;
}

QStringList VideoOutputVDPAU::GetVisualiserList(void)
{
    if (m_render)
        return VideoVisual::GetVisualiserList(m_render->Type());
    return VideoOutput::GetVisualiserList();
}

void VideoOutputVDPAU::SetVideoFlip(void)
{
    if (!m_render)
    {
        LOG(VB_PLAYBACK, LOG_ERR, QString("SetVideoFlip failed."));
        return;
    }
    m_render->SetVideoFlip();
}
