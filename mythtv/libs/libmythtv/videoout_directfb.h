#ifndef VIDEOOUT_DIRECTFB_H
#define VIDEOOUT_DIRECTFB_H

#include <qobject.h>

struct DirectfbData;

#include "videooutbase.h"

extern "C" {
#include <directfb.h>
}

class NuppelVideoPlayer;

class VideoOutputDirectfb: public VideoOutput
{
  public:
    VideoOutputDirectfb();
    ~VideoOutputDirectfb();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    void PrepareFrame(VideoFrame *buffer);
    void Show(void);

    void InputChanged(int width, int height, float aspect);
    void AspectChanged(float aspect);

    //void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    //void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawUnusedRects(void);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

  private:

	QObject *widget;

    DirectfbData *data;

    bool XJ_started;

    VideoFrame *scratchFrame;
    VideoFrame pauseFrame;

    void Exit(void);
    bool CreateDirectfbBuffers(void);
    void DeleteDirectfbBuffers(void);
};

DFBEnumerationResult LayerCallback(unsigned int id,
  DFBDisplayLayerDescription desc, void *data);

#endif
