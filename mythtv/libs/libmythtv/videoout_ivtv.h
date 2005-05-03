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

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void Show(FrameScanType );

    void InputChanged(int width, int height, float aspect);

    int GetRefreshRate(void);

    void DrawUnusedRects(bool sync = true);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

    int WriteBuffer(unsigned char *buf, int count);
    int Poll(int delay);
    void Pause(void);
    void Start(int skip, int mute);
    void Stop(bool hide);

    void Open(void);
    void Close(void);

    void SetFPS(float lfps) { fps = lfps; }

    void ClearOSD(void);

    bool Play(float speed, bool normal, int mask);
    bool Play(void) { return Play(last_speed, last_normal, last_mask); };
    void NextPlay(float speed, bool normal, int mask)
        { last_speed = speed; last_normal = normal; last_mask = mask; };
    void Flush(void);
    void Step(void);
    int GetFramesPlayed(void);

  private:
    typedef enum { kAlpha_Solid, kAlpha_Local, kAlpha_Clear, kAlpha_Embedded } eAlphaState;

    void ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer);
    void SetAlpha(eAlphaState newAlpha);
 
    int videofd;
    int fbfd;

    float fps;
    QString videoDevice;
    unsigned driver_version;

    QMutex lock;

    int mapped_offset;
    int mapped_memlen;
    char *mapped_mem;
    char *pixels;

    int stride;

    bool lastcleared;

    char *osdbuffer;
    char *osdbuf_aligned;

    int osdbufsize;

    float last_speed;
    bool last_normal;
    int last_mask;
    eAlphaState alphaState;
};

#endif
