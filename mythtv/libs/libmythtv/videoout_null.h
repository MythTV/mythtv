#ifndef VIDEOOUT_NULL_H_
#define VIDEOOUT_NULL_H_

#include "videooutbase.h"

class VideoOutputNull : public VideoOutput
{
  public:
    VideoOutputNull();
   ~VideoOutputNull();

    bool Init(int width, int height, float aspect, unsigned int winid,
              int winx, int winy, int winw, int winh, unsigned int embedid = 0);
    void PrepareFrame(VideoFrame *buffer);
    void Show(void);

    void InputChanged(int width, int height, float aspect);
    void AspectChanged(float aspect);

    void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawUnusedRects(void);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      vector<VideoFilter *> &filterList,
                      NuppelVideoPlayer *pipPlayer);

  private:
    void Exit(void);
    bool CreateNullBuffers(void);
    void DeleteNullBuffers(void);

    bool XJ_started;

    VideoFrame *scratchFrame;
    VideoFrame pauseFrame;
};

#endif
