#ifndef VIDEOOUT_XVMC_H_
#define VIDEOOUT_XVMC_H_

struct XvMCData;

#include "videooutbase.h"

class VideoOutputXvMC : public VideoOutput
{
  public:
    VideoOutputXvMC();
   ~VideoOutputXvMC();

    bool Init(int width, int height, float aspect, int num_buffers, 
              VideoFrame *out_buffers, unsigned int winid,
              int winx, int winy, int winw, int winh, unsigned int embedid = 0);
    void PrepareFrame(VideoFrame *buffer);
    void Show(void);

    void InputChanged(int width, int height, float aspect);
    void AspectChanged(float aspect);

    void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    void DrawUnusedRects(void);

  private:
    void Exit(void);
    bool CreateXvMCBuffers(void);
    void DeleteXvMCBuffers(void);

    XvMCData *data;

    int XJ_screen_num;
    unsigned long XJ_white,XJ_black;
    int XJ_started;
    int XJ_depth;
    int XJ_caught_error;
    int XJ_screenx, XJ_screeny;
    int XJ_screenwidth, XJ_screenheight;
    int XJ_fullscreen;

    int xv_port;
    int colorid;

    pthread_mutex_t lock;

    int colorkey;
};

#endif
