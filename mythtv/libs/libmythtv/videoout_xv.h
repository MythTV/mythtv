#ifndef VIDEOOUT_XV_H_
#define VIDEOOUT_XV_H_

struct XvData;

#include <DisplayRes.h>
#include "videooutbase.h"

class NuppelVideoPlayer;

class VideoOutputXv : public VideoOutput
{
  public:
    VideoOutputXv();
   ~VideoOutputXv();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    bool ApproveDeintFilter(const QString& filtername) const;
    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void Show(FrameScanType );

    void InputChanged(int width, int height, float aspect);
    void Zoom(int direction);
    void AspectChanged(float aspect);

    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawUnusedRects(void);

    float GetDisplayAspect(void);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

    int ChangePictureAttribute(int attributeType, int newValue);

  private:
    void Exit(void);
    bool CreateXvBuffers(void);
    bool CreateShmBuffers(void);
    bool CreateXBuffers(void);
    void DeleteXvBuffers(void);
    void DeleteShmBuffers(void);
    void DeleteXBuffers(void);

    DisplayRes * display_res;

    XvData *data;

    int XJ_screen_num;
    unsigned long XJ_white,XJ_black;
    int XJ_started;
    int XJ_depth;
    int XJ_caught_error;
    int XJ_screenx, XJ_screeny;
    int XJ_screenwidth, XJ_screenheight;

    int xv_port;
    int colorid;

    int use_shm;

    long long framesShown;
    int showFrame;
    int fps;
    time_t stop_time;

    unsigned char *scratchspace;

    pthread_mutex_t lock;

    VideoFrame *scratchFrame;
    VideoFrame pauseFrame;
};

#endif
