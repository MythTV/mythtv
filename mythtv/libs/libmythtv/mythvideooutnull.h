#ifndef MYTH_VIDEOOUT_NULL_H_
#define MYTH_VIDEOOUT_NULL_H_

// MythTV
#include "mythvideoout.h"

class MythVideoOutputNull : public MythVideoOutput
{
  public:
    static void GetRenderOptions(RenderOptions &Options);
    MythVideoOutputNull();
   ~MythVideoOutputNull() override;

    bool Init(const QSize &video_dim_buf, const QSize &video_dim_disp,
              float aspect, MythDisplay *Display,
              const QRect &win_rect, MythCodecID codec_id) override;
    void SetDeinterlacing(bool Enable, bool DoubleRate, MythDeintType Force = DEINT_NONE) override;
    void PrepareFrame(VideoFrame *buffer, FrameScanType, OSD *osd) override; // VideoOutput
    void Show(FrameScanType ) override; // VideoOutput
    void CreatePauseFrame(void);
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      bool        &aspect_only,
                      MythMultiLocker* Locks,
                      int ReferenceFrames,
                      bool ForceChange) override; // VideoOutput
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
    QMutex     m_globalLock   {QMutex::Recursive};
    VideoFrame m_avPauseFrame;
};
#endif
