#ifndef VIDEOOUT_XV_H_
#define VIDEOOUT_XV_H_

struct XvData;

#include "videooutbase.h"

class VideoOutputXv : public VideoOutput
{
  public:
    VideoOutputXv();
   ~VideoOutputXv();

    bool Init(int width, int height, float aspect, int num_buffers, 
              VideoFrame *out_buffers, unsigned int winid,
              int winx, int winy, int winw, int winh, unsigned int embedid = 0);
    void PrepareFrame(VideoFrame *buffer);
    void Show(void);

    void InputChanged(int width, int height, float aspect,
                      int num_buffers, VideoFrame *out_buffer);

    void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

  private:
    void Exit(void);
    bool CreateXvBuffers(int num_buffers, VideoFrame *out_buffers);
    bool CreateShmBuffers(int num_buffers, VideoFrame *out_buffers);
    bool CreateXBuffers(int num_buffers, VideoFrame *out_buffers);
    void DeleteXvBuffers();
    void DeleteShmBuffers();
    void DeleteXBuffers();

    XvData *data;

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

    int use_shm;

    unsigned char *scratchspace;

    pthread_mutex_t lock;
};

#endif
