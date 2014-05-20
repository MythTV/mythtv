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
              WId winid, const QRect &win_rect, MythCodecID codec_id);
    virtual void* GetDecoderContext(unsigned char* buf, uint8_t*& id);
    bool SetDeinterlacingEnabled(bool interlaced);
    bool SetupDeinterlace(bool interlaced, const QString& ovrf="");
    bool ApproveDeintFilter(const QString& filtername) const;
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);
    void PrepareFrame(VideoFrame*, FrameScanType, OSD *osd);
    void DrawSlice(VideoFrame*, int x, int y, int w, int h);
    static VdpStatus Render(VdpDecoder decoder, VdpVideoSurface target,
                            VdpPictureInfo const *picture_info,
                            uint32_t bitstream_buffer_count,
                            VdpBitstreamBuffer const *bitstream_buffers);
    void Show(FrameScanType);
    void ClearAfterSeek(void);
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only);
    void Zoom(ZoomDirection direction);
    void VideoAspectRatioChanged(float aspect);
    void EmbedInWidget(const QRect &rect);
    void StopEmbedding(void);
    void MoveResizeWindow(QRect new_rect);
    void DrawUnusedRects(bool sync = true);
    void UpdatePauseFrame(int64_t &disp_timecode);
    int  SetPictureAttribute(PictureAttribute attribute, int newValue);
    void InitPictureAttributes(void);
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                    const QSize &video_dim);
    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             const QString &decoder,
                                             uint stream_type,
                                             bool no_acceleration);
    static bool IsNVIDIA(void);
    virtual bool IsPIPSupported(void) const { return true;  }
    virtual bool IsPBPSupported(void) const { return false; }
    virtual bool NeedExtraAudioDecode(void) const
        { return codec_is_vdpau(video_codec_id); }
    virtual bool hasHWAcceleration(void) const
        { return codec_is_vdpau(video_codec_id); }
    virtual MythPainter *GetOSDPainter(void);
    virtual bool GetScreenShot(int width = 0, int height = 0,
                               QString filename = "");

    virtual bool CanVisualise(AudioPlayer *audio, MythRender *render)
        { return VideoOutput::CanVisualise(audio, m_render);       }
    virtual bool SetupVisualisation(AudioPlayer *audio, MythRender *render,
                                    const QString &name)
        { return VideoOutput::SetupVisualisation(audio, m_render, name); }
    virtual QStringList GetVisualiserList(void);
    virtual void ClearDummyFrame(VideoFrame* frame);
    virtual void SetVideoFlip(void);

  private:
    virtual bool hasFullScreenOSD(void) const { return true; }
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
    void DiscardFrame(VideoFrame*);
    void DiscardFrames(bool next_frame_keyframe);
    void DoneDisplayingFrame(VideoFrame *frame);
    void CheckFrameStates(void);
    virtual void ShowPIP(VideoFrame        *frame,
                         MythPlayer *pipplayer,
                         PIPLocation        loc);
    virtual void RemovePIP(MythPlayer *pipplayer);
    bool InitPIPLayer(QSize size);
    void DeinitPIPS(void);
    void DeinitPIPLayer(void);
    void ParseOptions(void);

    Window               m_win;
    MythRenderVDPAU     *m_render;
    AVVDPAUContext       m_context;

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
