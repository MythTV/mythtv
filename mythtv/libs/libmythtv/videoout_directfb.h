#ifndef VIDEOOUT_DIRECTFB_H
#define VIDEOOUT_DIRECTFB_H

#include <qobject.h>
#include "videooutbase.h"

class NuppelVideoPlayer;
class DirectfbData;

class VideoOutputDirectfb: public VideoOutput
{
  public:
    VideoOutputDirectfb();
    ~VideoOutputDirectfb();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);

    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);
    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void Show(FrameScanType);

    void MoveResize(void);
    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private);
    void Zoom(ZoomDirection direction);
    void DrawUnusedRects(bool /*sync*/) { }
    void UpdatePauseFrame(void);
    int  SetPictureAttribute(PictureAttribute attribute, int newValue);

    int  GetRefreshRate(void);

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

  private:
    bool          XJ_started;
    VideoFrame    pauseFrame;
    QObject      *widget;
    DirectfbData *data;
};

#endif
