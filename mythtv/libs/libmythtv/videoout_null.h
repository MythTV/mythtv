#ifndef VIDEOOUT_NULL_H_
#define VIDEOOUT_NULL_H_

#include "videooutbase.h"

class VideoOutputNull : public VideoOutput
{
  public:
    VideoOutputNull();
   ~VideoOutputNull();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);

    bool SetupDeinterlace(bool, const QString &ovrf = "")
        { (void)ovrf; return false; } // we don't deinterlace in null output..

    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void Show(FrameScanType );

    void InputChanged(int width, int height, float aspect);
    void Zoom(int direction);

    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawUnusedRects(bool sync = true);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

  private:
    void Exit(void);

    bool XJ_started;

    VideoFrame pauseFrame;
};

#endif
