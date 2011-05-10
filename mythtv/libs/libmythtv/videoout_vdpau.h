#ifndef VIDEOOUT_VDPAU_H
#define VIDEOOUT_VDPAU_H

// MythTV headers
#include "videooutbase.h"
#include "mythrender_vdpau.h"

class VDPAUContext;
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
    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh,
              MythCodecID codec_id, WId embedid = 0);
    bool SetDeinterlacingEnabled(bool interlaced);
    bool SetupDeinterlace(bool interlaced, const QString& ovrf="");
    bool ApproveDeintFilter(const QString& filtername) const;
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);
    void PrepareFrame(VideoFrame*, FrameScanType, OSD *osd);
    void DrawSlice(VideoFrame*, int x, int y, int w, int h);
    void Show(FrameScanType);
    void ClearAfterSeek(void);
    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only);
    void Zoom(ZoomDirection direction);
    void VideoAspectRatioChanged(float aspect);
    void EmbedInWidget(int x, int y, int w, int h);
    void StopEmbedding(void);
    void MoveResizeWindow(QRect new_rect);
    void DrawUnusedRects(bool sync = true);
    void UpdatePauseFrame(void);
    int  SetPictureAttribute(PictureAttribute attribute, int newValue);
    void InitPictureAttributes(void);
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                    const QSize &video_dim);
    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             uint stream_type,
                                             bool no_acceleration);
    virtual bool IsPIPSupported(void) const { return true;  }
    virtual bool IsPBPSupported(void) const { return false; }
    virtual bool NeedExtraAudioDecode(void) const
        { return codec_is_vdpau(video_codec_id); }
    virtual bool hasHWAcceleration(void) const
        { return codec_is_vdpau(video_codec_id); }
    virtual bool IsSyncLocked(void) const { return true; }
    void SetNextFrameDisplayTimeOffset(int delayus) { m_frame_delay = delayus; }
    virtual MythPainter* GetOSDPainter(void) { return (MythPainter*)m_osd_painter; }

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

    uint                 m_buffer_size;
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

    int                  m_frame_delay;
    QMutex               m_lock;

    uint                 m_pip_layer;
    uint                 m_pip_surface;
    bool                 m_pip_ready;
    QMap<MythPlayer*,vdpauPIP> m_pips;

    MythVDPAUPainter    *m_osd_painter;

    bool                 m_using_piccontrols;
    bool                 m_skip_chroma;
    float                m_denoise;
    float                m_sharpen;
    bool                 m_studio;
    int                  m_colorspace;
};

#endif // VIDEOOUT_VDPAU_H


