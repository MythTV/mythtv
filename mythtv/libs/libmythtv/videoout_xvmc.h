#ifndef VIDEOOUT_XVMC_H_
#define VIDEOOUT_XVMC_H_

struct XvMCData;

#include <DisplayRes.h>
#include "videooutbase.h"

class VideoOutputXvMC : public VideoOutput
{
  public:
    VideoOutputXvMC();
   ~VideoOutputXvMC();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    bool NeedsDoubleFramerate() const;
    bool ApproveDeintFilter(const QString& filtername) const;
    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void Show(FrameScanType scan);

    void InputChanged(int width, int height, float aspect);
    void Zoom(int direction);
    void AspectChanged(float aspect);

    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    void DrawUnusedRects(void);

    float GetDisplayAspect(void);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

    int ChangePictureAttribute(int attributeType, int newValue);

    bool hasIDCTAcceleration() const;

  private:
    void Exit(void);
    bool CreateXvMCBuffers(void);
    void DeleteXvMCBuffers(void);

    DisplayRes * display_res;

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
    int chroma;
};

#endif
