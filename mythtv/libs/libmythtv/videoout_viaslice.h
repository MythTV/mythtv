#ifndef VIDEOOUT_VIA_H_
#define VIDEOOUT_VIA_H_

struct ViaData;

#include "videooutbase.h"

class NuppelVideoPlayer;

class VideoOutputVIA : public VideoOutput
{
  public:
    VideoOutputVIA();
   ~VideoOutputVIA();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void Show(FrameScanType );

    void InputChanged(int width, int height, float aspect);
    void AspectChanged(float aspect);
    void Zoom(int direction);

    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    float GetDisplayAspect(void);

    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    void DrawUnusedRects(void);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

  private:
    bool CreateViaBuffers(void);
    void DeleteViaBuffers(void);
    void Exit(void);

    ViaData *data;

    int XJ_screen_num;
    unsigned long XJ_white,XJ_black;
    int XJ_started;
    int XJ_depth;
    int XJ_caught_error;
    int XJ_screenx, XJ_screeny;
    int XJ_screenwidth, XJ_screenheight;
    int XJ_fullscreen;

    pthread_mutex_t lock;

    int colorkey;
};

#endif
