#ifndef VIDEOOUT_NULL_H_
#define VIDEOOUT_NULL_H_

#include <QMutex>

#include "videooutbase.h"

class VideoOutputNull : public VideoOutput
{
  public:
    static void GetRenderOptions(RenderOptions &Options);
    VideoOutputNull();
   ~VideoOutputNull();

    bool Init(const QSize &video_dim_buf,
              const QSize &video_dim_disp,
              float aspect,
              WId winid, const QRect &win_rect, MythCodecID codec_id) override; // VideoOutput
    void SetDeinterlacing(bool Enable, bool DoubleRate) override;
    void PrepareFrame(VideoFrame *buffer, FrameScanType, OSD *osd) override; // VideoOutput
    void Show(FrameScanType ) override; // VideoOutput
    void CreatePauseFrame(void);
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      bool        &aspect_only,
                      MythMultiLocker* Locks,
                      int ReferenceFrames) override; // VideoOutput
    void EmbedInWidget(const QRect &rect) override; // VideoOutput
    void StopEmbedding(void) override; // VideoOutput
    void UpdatePauseFrame(int64_t &disp_timecode) override; // VideoOutput
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      const PIPMap &pipPlayers,
                      FrameScanType scan) override; // VideoOutput
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    bool SetupVisualisation(AudioPlayer *, MythRender *,
                            const QString &) override // VideoOutput
        { return false; }

  private:
    QMutex     global_lock;
    VideoFrame av_pause_frame;
};
#endif
