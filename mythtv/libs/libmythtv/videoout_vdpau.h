#ifndef VIDEOOUT_VDPAU_H
#define VIDEOOUT_VDPAU_H

// MythTV headers
#include "videooutbase.h"
#include "mythrender_vdpau.h"

class MythVDPAUPainter;
class OSD;

struct vdpauPIP
{
    QSize videoSize;
    uint  videoSurface;
    uint  videoMixer;
};

class VideoOutputVDPAU : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts);
    VideoOutputVDPAU();
    ~VideoOutputVDPAU();
    bool Init(const QSize &video_dim_buf,
              const QSize &video_dim_disp,
              float aspect,
              WId winid, const QRect &win_rect, MythCodecID codec_id) override; // VideoOutput
    void* GetDecoderContext(unsigned char* buf, uint8_t*& id) override; // VideoOutput
    bool SetDeinterlacingEnabled(bool interlaced) override; // VideoOutput
    bool SetupDeinterlace(bool interlaced, const QString& overridefilter="") override; // VideoOutput
    bool ApproveDeintFilter(const QString& filtername) const override; // VideoOutput
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan) override; // VideoOutput
    void PrepareFrame(VideoFrame*, FrameScanType, OSD *osd) override; // VideoOutput
    void DrawSlice(VideoFrame*, int x, int y, int w, int h) override; // VideoOutput
    void Show(FrameScanType) override; // VideoOutput
    void ClearAfterSeek(void) override; // VideoOutput
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only) override; // VideoOutput
    void Zoom(ZoomDirection direction) override; // VideoOutput
    void VideoAspectRatioChanged(float aspect) override; // VideoOutput
    void EmbedInWidget(const QRect &rect) override; // VideoOutput
    void StopEmbedding(void) override; // VideoOutput
    void MoveResizeWindow(QRect new_rect) override; // VideoOutput
    void DrawUnusedRects(bool sync = true) override; // VideoOutput
    void UpdatePauseFrame(int64_t &disp_timecode) override; // VideoOutput
    int  SetPictureAttribute(PictureAttribute attribute, int newValue) override; // VideoOutput
    void InitPictureAttributes(void) override; // VideoOutput
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                    const QSize &video_dim);
    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             const QString &decoder,
                                             uint stream_type,
                                             bool no_acceleration);
    static bool IsNVIDIA(void);
    bool IsPIPSupported(void) const override // VideoOutput
        { return true;  }
    bool IsPBPSupported(void) const override // VideoOutput
        { return false; }
    bool NeedExtraAudioDecode(void) const override // VideoOutput
        { return codec_is_vdpau(video_codec_id); }
    bool hasHWAcceleration(void) const override // VideoOutput
        { return codec_is_vdpau(video_codec_id); }
    MythPainter *GetOSDPainter(void) override; // VideoOutput
    bool GetScreenShot(int width = 0, int height = 0,
                       QString filename = "") override; // VideoOutput

    bool CanVisualise(AudioPlayer *audio, MythRender */*render*/) override // VideoOutput
        { return VideoOutput::CanVisualise(audio, m_render);       }
    bool SetupVisualisation(AudioPlayer *audio, MythRender */*render*/,
                            const QString &name) override // VideoOutput
        { return VideoOutput::SetupVisualisation(audio, m_render, name); }
    QStringList GetVisualiserList(void) override; // VideoOutput
    void ClearDummyFrame(VideoFrame* frame) override; // VideoOutput
    void SetVideoFlip(void) override; // VideoOutput
    MythRenderVDPAU* getRender() const { return m_render; }

  private:
    bool hasFullScreenOSD(void) const override // VideoOutput
        { return true; }
    void TearDown(void);
    bool InitRender(void);
    void DeleteRender(void);
    bool InitBuffers(void);
    bool CreateVideoSurfaces(uint num);
    void ClaimVideoSurfaces(void);
    void DeleteVideoSurfaces(void);
    void DeleteBuffers(void);
    void RestoreDisplay(void);
    void UpdateReferenceFrames(VideoFrame *frame);
    bool FrameIsInUse(VideoFrame *frame);
    void ClearReferenceFrames(void);
    void DiscardFrame(VideoFrame*) override; // VideoOutput
    void DiscardFrames(bool next_frame_keyframe) override; // VideoOutput
    void DoneDisplayingFrame(VideoFrame *frame) override; // VideoOutput
    void CheckFrameStates(void) override; // VideoOutput
    void ShowPIP(VideoFrame   *frame,
                 MythPlayer   *pipplayer,
                 PIPLocation   loc) override; // VideoOutput
    void RemovePIP(MythPlayer *pipplayer) override; // VideoOutput
    bool InitPIPLayer(QSize size);
    void DeinitPIPS(void);
    void DeinitPIPLayer(void);
    void ParseOptions(void);

    Window               m_win;
    MythRenderVDPAU     *m_render;
    AVVDPAUContext      *m_context;

    uint                 m_decoder_buffer_size;
    uint                 m_process_buffer_size;
    QVector<uint>        m_video_surfaces;
    QVector<uint>        m_reference_frames;
    uint                 m_pause_surface;
    bool                 m_need_deintrefs;
    uint                 m_video_mixer;
    uint                 m_mixer_features;
    bool                 m_checked_surface_ownership;
    bool                 m_checked_output_surfaces;

    uint                 m_decoder;
    int                  m_pix_fmt;

    QMutex               m_lock;

    uint                 m_pip_layer;
    uint                 m_pip_surface;
    bool                 m_pip_ready;
    QMap<MythPlayer*,vdpauPIP> m_pips;

    MythVDPAUPainter    *m_osd_painter;

    bool                 m_skip_chroma;
    float                m_denoise;
    float                m_sharpen;
    int                  m_colorspace;
};

#endif // VIDEOOUT_VDPAU_H
