#ifndef VIDEOOUT_XV_H_
#define VIDEOOUT_XV_H_

struct XvData;

#include "videooutbase.h"

class NuppelVideoPlayer;

class VideoOutputXv : public VideoOutput
{
  public:
    VideoOutputXv();
   ~VideoOutputXv();

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

    int ChangePictureAttribute(int attributeType, int newValue);

  private:
    void Exit(void);
    bool CreateXvBuffers(void);
    bool CreateShmBuffers(void);
    bool CreateXBuffers(void);
    void DeleteXvBuffers(void);
    void DeleteShmBuffers(void);
    void DeleteXBuffers(void);

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

    unsigned char *scratchspace;

    pthread_mutex_t lock;

    VideoFrame *scratchFrame;
    VideoFrame pauseFrame;
};

#endif
