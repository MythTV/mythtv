#ifndef VIDEOOUT_IVTV_H_
#define VIDEOOUT_IVTV_H_

#include <qstring.h>
#include <qmutex.h>

#include "videooutbase.h"

class NuppelVideoPlayer;

class VideoOutputIvtv: public VideoOutput
{
  public:
    VideoOutputIvtv();
   ~VideoOutputIvtv();

    bool Init(int width, int height, float aspect, unsigned int winid,
              int winx, int winy, int winw, int winh, unsigned int embedid = 0);
    void PrepareFrame(VideoFrame *buffer);
    void Show(void);

    void InputChanged(int width, int height, float aspect);

    void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawUnusedRects(void);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      vector<VideoFilter *> &filterList,
                      NuppelVideoPlayer *pipPlayer);

    int WriteBuffer(unsigned char *buf, int count);
    unsigned long FrameSync();
    void Pause(void);
    void Play(void);

    void Reopen(int skipframes = 0);

    void SetFPS(float lfps) { fps = lfps; }

  private:
    int videofd;
    int writefd;

    float fps;
    QString videoDevice;

    QMutex lock;
};

#endif
