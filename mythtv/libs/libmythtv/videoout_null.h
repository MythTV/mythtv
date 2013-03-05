#ifndef VIDEOOUT_NULL_H_
#define VIDEOOUT_NULL_H_

#include <QMutex>

#include "videooutbase.h"

class VideoOutputNull : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    VideoOutputNull();
   ~VideoOutputNull();

    bool Init(const QSize &video_dim_buf,
              const QSize &video_dim_disp,
              float aspect,
              WId winid, const QRect &win_rect, MythCodecID codec_id);
    bool SetupDeinterlace(bool, const QString &ovrf = "")
        { (void)ovrf; return false; } // we don't deinterlace in null output..
    void PrepareFrame(VideoFrame *buffer, FrameScanType, OSD *osd);
    void Show(FrameScanType );
    void CreatePauseFrame(void);
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only);
    void Zoom(ZoomDirection direction);
    void EmbedInWidget(const QRect &rect);
    void StopEmbedding(void);
    void DrawUnusedRects(bool sync = true);
    void UpdatePauseFrame(int64_t &disp_timecode);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    void MoveResizeWindow(QRect ) {;}
    virtual bool SetupVisualisation(AudioPlayer *audio, MythRender *render,
                                    const QString &name) { return false; }

  private:
    QMutex     global_lock;
    VideoFrame av_pause_frame;
};
#endif
