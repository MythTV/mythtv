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
#define LOC_ERR  QString("VidOutVDPAU Error: ")
#define NUM_VDPAU_BUFFERS 17

#define CHECK_ERROR(Loc) \
  if (m_render && m_render->IsErrored()) \
      errorState = kError_Unknown; \
  if (IsErrored()) \
  { \
      VERBOSE(VB_IMPORTANT, LOC_ERR + QString("IsErrored() in %1").arg(Loc)); \
      return; \
  }

void VideoOutputVDPAU::GetRenderOptions(render_opts &opts)
{
    opts.renderers->append("vdpau");
    (*opts.osds)["vdpau"].append("vdpau");
    if (opts.decoders->contains("vdpau"))
        (*opts.safe_renderers)["vdpau"].append("vdpau");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("vdpau");
    if (opts.decoders->contains("libmpeg2"))
        (*opts.safe_renderers)["libmpeg2"].append("vdpau");
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
    m_buffer_size(NUM_VDPAU_BUFFERS),          m_pause_surface(0),
    m_need_deintrefs(false), m_video_mixer(0), m_mixer_features(kVDPFeatNone),
    m_checked_surface_ownership(false),
    m_checked_output_surfaces(false),
    m_decoder(0),            m_pix_fmt(-1),    m_frame_delay(0),
    m_lock(QMutex::Recursive), m_pip_layer(0), m_pip_surface(0),
    m_pip_ready(false),      m_osd_painter(NULL), m_using_piccontrols(false),
    m_skip_chroma(false),    m_denoise(0.0f),
    m_sharpen(0.0f),         m_studio(false),
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

bool VideoOutputVDPAU::Init(int width, int height, float aspect, WId winid,
                            int winx, int winy, int winw, int winh,
                            MythCodecID codec_id, WId embedid)
{
    // Attempt to free up as much video memory as possible
    // only works when using the VDPAU painter for the UI
    MythPainter *painter = GetMythPainter();
    if (painter)
        painter->FreeResources();

    (void) embedid;
    m_win = winid;
    QMutexLocker locker(&m_lock);
    window.SetNeedRepaint(true);
    bool ok = VideoOutput::Init(width, height, aspect,
                                winid, winx, winy, winw, winh,
                                codec_id, embedid);
    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("vdpau");

    InitDisplayMeasurements(width, height, true);
    ParseOptions();
    if (ok) ok = InitRender();
    if (ok) ok = InitBuffers();
    if (!ok)
    {
        TearDown();
        return ok;
    }

    m_using_piccontrols = (db_use_picture_controls || m_studio ||
                           m_colorspace != VDP_COLOR_STANDARD_ITUR_BT_601);
    if (m_using_piccontrols)
        InitPictureAttributes();
    MoveResize();
    VERBOSE(VB_PLAYBACK, LOC +
            QString("Created VDPAU context (%1 decode)")
            .arg(codec_is_std(video_codec_id) ? "software" : "GPU"));

    return ok;
}

bool VideoOutputVDPAU::InitRender(void)
{
    QMutexLocker locker(&m_lock);

    const QSize size = window.GetDisplayVisibleRect().size();
    const QRect rect = QRect(QPoint(0,0), size);
    m_render = new MythRenderVDPAU();

    if (m_render && m_render->Create(size, m_win))
    {
        m_render->SetMaster(kMasterVideo);
        m_osd_painter = new MythVDPAUPainter(m_render);
        if (m_osd_painter)
        {
            m_osd_painter->SetSwapControl(false);
            VERBOSE(VB_PLAYBACK, LOC + QString("Created VDPAU osd (%1x%2)")
                .arg(size.width()).arg(size.height()));
        }
        else
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to create VDPAU osd."));
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to initialise VDPAU"));

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

        delete m_render;
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
    const QSize video_dim = codec_is_std(video_codec_id) ?
                            window.GetVideoDim() : window.GetActualVideoDim();

    vbuffers.Init(m_buffer_size, false, 2, 1, 4, 1, false);

    bool ok = false;
    if (codec_is_vdpau(video_codec_id))
    {
        ok = CreateVideoSurfaces(m_buffer_size);
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unable to create VDPAU buffers"));
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unable to create VDPAU mixer"));
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
    if (!m_render || !m_video_surfaces.size())
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
                VERBOSE(VB_PLAYBACK, LOC + QString("Enabled deinterlacing."));
            }
            else
            {
                enable = false;
                VERBOSE(VB_PLAYBACK, LOC +
                                    QString("Failed to enable deinterlacing."));
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
    CHECK_ERROR("ProcessFrame")

    if (!m_checked_surface_ownership && codec_is_std(video_codec_id))
        ClaimVideoSurfaces();

    m_pip_ready = false;
    ShowPIPs(frame, pipPlayers);
}

void VideoOutputVDPAU::PrepareFrame(VideoFrame *frame, FrameScanType scan,
                                    OSD *osd)
{
    QMutexLocker locker(&m_lock);
    (void)osd;
    CHECK_ERROR("PrepareFrame")

    if (!m_render)
        return;

    if (!m_checked_output_surfaces &&
        !(!codec_is_std(video_codec_id) && !m_decoder))
    {
        m_render->SetMaster(kMasterVideo);
        m_render->CheckOutputSurfaces();
        m_checked_output_surfaces = true;
    }

    bool new_frame = false;
    if (frame)
    {
        // FIXME for 0.23. This should be triggered from AFD by a seek
        if ((abs(frame->frameNumber - framesPlayed) > 8))
            ClearReferenceFrames();
        new_frame = (framesPlayed != frame->frameNumber + 1);
        framesPlayed = frame->frameNumber + 1;
    }

    uint video_surface = m_video_surfaces[0];
    bool deint = (m_deinterlacing && m_need_deintrefs && frame);

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
    else if (new_frame && frame)
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Unexpected display size."));

    if (!m_render->MixAndRend(m_video_mixer, field, video_surface, 0,
                              deint ? &m_reference_frames : NULL,
                              scan == kScan_Interlaced,
                              window.GetVideoRect(),
                              QRect(QPoint(0,0), size),
                              vsz_enabled ? vsz_desired_display_rect :
                                            window.GetDisplayVideoRect(),
                              m_pip_ready ? m_pip_layer : 0, 0))
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Prepare frame failed."));

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

    CHECK_ERROR("DrawSlice")

    if (codec_is_std(video_codec_id) || !m_render)
        return;

    if (!m_checked_surface_ownership)
        ClaimVideoSurfaces();

    struct vdpau_render_state *render = (struct vdpau_render_state *)frame->buf;
    if (!render)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("No video surface to decode to."));
        errorState = kError_Unknown;
        return;
    }

    if (frame->pix_fmt != m_pix_fmt)
    {
        if (m_decoder)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Picture format has changed."));
            errorState = kError_Unknown;
            return;
        }

        uint max_refs = 2;
        if (frame->pix_fmt == PIX_FMT_VDPAU_H264)
        {
            max_refs = render->info.h264.num_ref_frames;
            if (max_refs < 1 || max_refs > 16)
            {
                uint32_t round_width  = (frame->width + 15) & ~15;
                uint32_t round_height = (frame->height + 15) & ~15;
                uint32_t surf_size    = (round_width * round_height * 3) / 2;
                max_refs = (12 * 1024 * 1024) / surf_size;
            }
            if (max_refs > 16)
                max_refs = 16;
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
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Picture format is not supported."));
                errorState = kError_Unknown;
                return;
        }

        m_decoder = m_render->CreateDecoder(window.GetActualVideoDim(),
                                            vdp_decoder_profile, max_refs);
        if (m_decoder)
        {
            m_pix_fmt = frame->pix_fmt;
            VERBOSE(VB_PLAYBACK, LOC +
                QString("Created VDPAU decoder (%1 ref frames)")
                .arg(max_refs));
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to create decoder."));
            errorState = kError_Unknown;
            return;
        }
    }
    else if (!m_decoder)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("Pix format already set but no VDPAU decoder."));
        errorState = kError_Unknown;
        return;
    }

    m_render->Decode(m_decoder, render);
}

void VideoOutputVDPAU::Show(FrameScanType scan)
{
    QMutexLocker locker(&m_lock);
    CHECK_ERROR("Show")

    if (window.IsRepaintNeeded())
        DrawUnusedRects(false);

    if (m_render)
    {
        m_render->Flip(m_frame_delay);
        m_frame_delay = 0;
    }
    CheckFrameStates();
}

void VideoOutputVDPAU::ClearAfterSeek(void)
{
    m_lock.lock();
    VERBOSE(VB_PLAYBACK, LOC + "ClearAfterSeek()");
    DiscardFrames(false);
    m_lock.unlock();
}

bool VideoOutputVDPAU::InputChanged(const QSize &input_size,
                                    float        aspect,
                                    MythCodecID  av_codec_id,
                                    void        *codec_private,
                                    bool        &aspect_only)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("InputChanged(%1,%2,%3) '%4'->'%5'")
            .arg(input_size.width()).arg(input_size.height()).arg(aspect)
            .arg(toString(video_codec_id)).arg(toString(av_codec_id)));

    QMutexLocker locker(&m_lock);
    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = input_size  != window.GetActualVideoDim();
    bool asp_changed = aspect      != window.GetVideoAspect();

    if (!res_changed && !cid_changed)
    {
        aspect_only = true;
        if (asp_changed)
        {
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        return true;
    }

    TearDown();
    QRect disp = window.GetDisplayVisibleRect();
    if (Init(input_size.width(), input_size.height(),
             aspect, m_win, disp.left(), disp.top(),
             disp.width(), disp.height(), av_codec_id, 0))
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
    if (!window.IsEmbedding())
    {
        VideoOutput::EmbedInWidget(x, y, w, h);
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

void VideoOutputVDPAU::UpdatePauseFrame(void)
{
    QMutexLocker locker(&m_lock);

    VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame() " + vbuffers.GetStatus());

    vbuffers.begin_lock(kVideoBuffer_used);

    if (vbuffers.size(kVideoBuffer_used) && m_render)
    {
        VideoFrame *frame = vbuffers.head(kVideoBuffer_used);
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
        VERBOSE(VB_PLAYBACK,LOC +
            QString("WARNING: Could not update pause frame - no used frames."));

    vbuffers.end_lock();
}

void VideoOutputVDPAU::InitPictureAttributes(void)
{
    supported_attributes = (PictureAttributeSupported)
                           (kPictureAttributeSupported_Brightness |
                            kPictureAttributeSupported_Contrast |
                            kPictureAttributeSupported_Colour |
                            kPictureAttributeSupported_Hue);
    VERBOSE(VB_PLAYBACK, LOC + QString("PictureAttributes: %1")
            .arg(toString(supported_attributes)));
    VideoOutput::InitPictureAttributes();

    m_lock.lock();
    if (m_render && m_video_mixer)
    {
        if (m_studio)
            m_render->SetMixerAttribute(m_video_mixer,
                                        kVDPAttribStudioLevels, 1);

        if (m_colorspace < 0)
        {
            QSize size = window.GetVideoDim();
            m_colorspace = (size.width() > 720 || size.height() > 576) ?
                            VDP_COLOR_STANDARD_ITUR_BT_709 :
                            VDP_COLOR_STANDARD_ITUR_BT_601;
            VERBOSE(VB_PLAYBACK, LOC + QString("Using ITU %1 colorspace")
                        .arg((m_colorspace == VDP_COLOR_STANDARD_ITUR_BT_601) ?
                        "BT.601" : "BT.709"));
        }

        if (m_colorspace != VDP_COLOR_STANDARD_ITUR_BT_601)
            m_render->SetMixerAttribute(m_video_mixer,
                                        kVDPAttribColorStandard,
                                        m_colorspace);
    }
    m_lock.unlock();
}

int VideoOutputVDPAU::SetPictureAttribute(PictureAttribute attribute,
                                          int newValue)
{
    if (!m_using_piccontrols || !m_render || !m_video_mixer)
        return -1;

    m_lock.lock();
    newValue = min(max(newValue, 0), 100);

    uint vdpau_attrib = kVDPAttribNone;
    switch (attribute)
    {
        case kPictureAttribute_Brightness:
            vdpau_attrib = kVDPAttribBrightness;
            break;
        case kPictureAttribute_Contrast:
            vdpau_attrib = kVDPAttribContrast;
            break;
        case kPictureAttribute_Colour:
            vdpau_attrib = kVDPAttribColour;
            break;
        case kPictureAttribute_Hue:
            vdpau_attrib = kVDPAttribHue;
            break;
        default:
            newValue = -1;
    }

    if (vdpau_attrib != kVDPAttribNone)
        newValue = m_render->SetMixerAttribute(m_video_mixer,
                                               vdpau_attrib, newValue);

    if (newValue >= 0)
        SetPictureAttributeDBValue(attribute, newValue);
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
    uint width,       uint height,
    uint stream_type, bool no_acceleration)
{
    bool use_cpu = no_acceleration;
    VideoDisplayProfile vdp;
    vdp.SetInput(QSize(width, height));
    QString dec = vdp.GetDecoder();

    MythCodecID test_cid = (MythCodecID)(kCodec_MPEG1_VDPAU + (stream_type-1));
    use_cpu |= !codec_is_vdpau_hw(test_cid);
    if (test_cid == kCodec_MPEG4_VDPAU)
        use_cpu |= !MythRenderVDPAU::IsMPEG4Available();
    if (test_cid == kCodec_H264_VDPAU)
        use_cpu |= !MythRenderVDPAU::H264DecoderSizeSupported(width, height);
    if ((dec != "vdpau") || getenv("NO_VDPAU") || use_cpu)
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
        vbuffers.safeEnqueue(kVideoBuffer_displayed, frame);
    else
    {
        vbuffers.DoneDisplayingFrame(frame);
    }
    m_lock.unlock();
}

void VideoOutputVDPAU::DiscardFrames(bool next_frame_keyframe)
{
    m_lock.lock();
    VERBOSE(VB_PLAYBACK, LOC + "DiscardFrames("<<next_frame_keyframe<<")");
    CheckFrameStates();
    ClearReferenceFrames();
    vbuffers.DiscardFrames(next_frame_keyframe);
    VERBOSE(VB_PLAYBACK, LOC + QString("DiscardFrames() 3: %1 -- done()")
            .arg(vbuffers.GetStatus()));
    m_lock.unlock();
}

void VideoOutputVDPAU::DoneDisplayingFrame(VideoFrame *frame)
{
    m_lock.lock();
    if (vbuffers.contains(kVideoBuffer_used, frame))
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
                VERBOSE(VB_IMPORTANT, LOC + QString("Created pip %1x%2")
                    .arg(pipVideoDim.width()).arg(pipVideoDim.height()));
        }

        if (m_pips.contains(pipplayer))
        {
            QRect rect = GetPIPRect(loc, pipplayer);

            if (!m_pip_ready)
                m_render->DrawBitmap(0, m_pip_surface, NULL, NULL, 0, 0, 0, 0);

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
                                       255, 0, 0, 0, true);

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
                m_render->DrawBitmap(0, m_pip_surface, NULL, &l, 255, 127);
                m_render->DrawBitmap(0, m_pip_surface, NULL, &t, 255, 127);
                m_render->DrawBitmap(0, m_pip_surface, NULL, &b, 255, 127);
                m_render->DrawBitmap(0, m_pip_surface, NULL, &r, 255, 127);
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

    if (m_pips[pipplayer].videoMixer)
        m_render->DestroyVideoMixer(m_pips[pipplayer].videoMixer);

    m_pips.remove(pipplayer);
    VERBOSE(VB_PLAYBACK, LOC + "Removed 1 PIP");

    if (m_pips.empty())
        DeinitPIPLayer();
}

void VideoOutputVDPAU::ParseOptions(void)
{
    m_skip_chroma = false;
    m_denoise     = 0.0f;
    m_sharpen     = 0.0f;
    m_studio      = false;
    m_colorspace  = VDP_COLOR_STANDARD_ITUR_BT_601;
    m_buffer_size = NUM_VDPAU_BUFFERS;
    m_mixer_features = kVDPFeatNone;

    QStringList list = GetFilters().split(",");
    if (list.empty())
        return;

    for (QStringList::Iterator i = list.begin(); i != list.end(); ++i)
    {
        QString name = (*i).section('=', 0, 0).toLower();
        QString opts = (*i).section('=', 1).toLower();

        if (!name.contains("vdpau"))
            continue;

        if (name.contains("vdpaubuffersize"))
        {
            uint num = opts.toUInt();
            if (6 <= num && num <= 50)
            {
                m_buffer_size = num;
                VERBOSE(VB_PLAYBACK, LOC +
                            QString("VDPAU video buffer size: %1 (default %2)")
                            .arg(m_buffer_size).arg(NUM_VDPAU_BUFFERS));
            }
        }
        else if (name.contains("vdpauivtc"))
        {
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Enabling VDPAU inverse telecine "
                            "(requires Basic or Advanced deinterlacer)"));
            m_mixer_features |= kVDPFeatIVTC;
        }
        else if (name.contains("vdpauskipchroma"))
        {
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Enabling SkipChromaDeinterlace."));
            m_skip_chroma = true;
        }
        else if (name.contains("vdpaudenoise"))
        {
            float tmp = std::max(0.0f, std::min(1.0f, opts.toFloat()));
            if (tmp != 0.0)
            {
                VERBOSE(VB_PLAYBACK, LOC +
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
                VERBOSE(VB_PLAYBACK, LOC +
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
                VERBOSE(VB_PLAYBACK, LOC +
                    QString("Forcing ITU BT.%1 colorspace")
                    .arg((m_colorspace == VDP_COLOR_STANDARD_ITUR_BT_601) ?
                    "BT.601" : "BT.709"));
            }
        }
        else if (name.contains("vdpaustudio"))
        {
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Enabling Studio Levels [16-235]."));
            m_studio = true;
        }
        else if (name.contains("vdpauhqscaling"))
        {
            m_mixer_features |= kVDPFeatHQScaling;
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Requesting high quality scaling."));
        }
    }
}
