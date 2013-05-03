#ifndef VIDEOOUT_QUARTZ_H_
#define VIDEOOUT_QUARTZ_H_

struct QuartzData;

#include "videooutbase.h"

class VideoOutputQuartz : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    VideoOutputQuartz();
   ~VideoOutputQuartz();

    bool Init(const QSize &video_dim_buf,
              const QSize &video_dim_disp,
              float aspect, WId winid,
              const QRect &win_rect, MythCodecID codec_id);

    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);
    void PrepareFrame(VideoFrame *buffer, FrameScanType t, OSD *osd);
    void Show(FrameScanType);

    void SetVideoFrameRate(float playback_fps);
    void ToggleAspectOverride(AspectOverrideMode aspectMode);
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only);
    void VideoAspectRatioChanged(float aspect);
    void MoveResize(void);
    void Zoom(ZoomDirection direction);
    void ToggleAdjustFill(AdjustFillMode adjustFill);

    void EmbedInWidget(const QRect &rect);
    void StopEmbedding(void);

    void MoveResizeWindow(QRect new_rect) {;}
    void DrawUnusedRects(bool sync = true);

    void UpdatePauseFrame(int64_t &disp_timecode);

    void ResizeForGui(void);
    void ResizeForVideo(uint width = 0, uint height = 0);

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

    static MythCodecID GetBestSupportedCodec(
        uint width, uint height,
        uint osd_width, uint osd_height,
        uint stream_type, uint fourcc);
    virtual bool NeedExtraAudioDecode(void) const
        { return !codec_is_std(video_codec_id); }

  private:
    void Exit(void);
    bool CreateQuartzBuffers(void);
    void DeleteQuartzBuffers(void);

    bool         Started;
    QuartzData  *data;
    VideoFrame   pauseFrame;
};

#endif
