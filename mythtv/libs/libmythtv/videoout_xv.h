#ifndef VIDEOOUT_XV_H_
#define VIDEOOUT_XV_H_

struct XvData;

#include "frame.h"

class VideoOutputXv
{
  public:
    VideoOutputXv();
   ~VideoOutputXv();

    bool Init(int width, int height, float aspect, int num_buffers, 
              VideoFrame *out_buffers, unsigned int winid,
              int winx, int winy, int winw, int winh, unsigned int embedid = 0);
    void PrepareFrame(VideoFrame *buffer);
    void Show();

    void InputChanged(int width, int height, float aspect,
                      int num_buffers, VideoFrame *out_buffer);

    void ToggleFullScreen();

    void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    void StopEmbedding(void);
    void ResizeVideo(int x, int y, int w, int h);
    void MoveResize(void);

    void GetDrawSize(int &xoff, int &yoff, int &width, int &height);

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
    int XJ_width, XJ_height;
    int XJ_screenx, XJ_screeny;
    int XJ_screenwidth, XJ_screenheight;
    int XJ_fullscreen;
    int XJ_fixedaspect;

    int oldx, oldy, oldw, oldh;
    int curx, cury, curw, curh;
    int img_xoff, img_yoff; 
    int imgx, imgy, imgw, imgh, dispxoff, dispyoff, dispwoff, disphoff;
    float img_hscanf, img_vscanf;

    int dispx, dispy, dispw, disph;
    int olddispx, olddispy, olddispw, olddisph;
    bool embedding;

    int xv_port;
    int colorid;

    int use_shm;

    unsigned char *scratchspace;

    pthread_mutex_t lock;

    float XJ_aspect;
};

#endif
