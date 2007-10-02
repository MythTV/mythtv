#ifndef VIDEOOUT_IVTV_H_
#define VIDEOOUT_IVTV_H_

#include <qstring.h>
#include <qmutex.h>

#include "videooutbase.h"

class NuppelVideoPlayer;
class VideoOutputIvtvPriv;

class VideoOutputIvtv: public VideoOutput
{
  public:
    VideoOutputIvtv();
   ~VideoOutputIvtv();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void Show(FrameScanType );

    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private);

    int GetRefreshRate(void);

    void DrawUnusedRects(bool sync = true);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

    uint WriteBuffer(unsigned char *buf, int count);
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
    long long GetFramesPlayed(void);

    VideoFrame *GetNextFreeFrame(bool with_lock = false,
                                 bool allow_unsafe = false)
    {
        (void) with_lock;
        (void) allow_unsafe;
        return NULL;
    }

    int ValidVideoFrames(void) const;

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

  private:
    typedef enum
    {
        kAlpha_Solid,
        kAlpha_Local,
        kAlpha_Clear,
        kAlpha_Embedded
    } eAlphaState;

    void ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer);
    void SetAlpha(eAlphaState newAlpha);
    void SetColorKey(int state, int color);
    long long GetFirmwareFramesPlayed(void);

    int videofd;
    int fbfd;

    float fps;
    QString videoDevice;
    unsigned int driver_version;
    bool has_v4l2_api;
    bool has_pause_bug;

    QMutex lock;

    int mapped_offset;
    int mapped_memlen;
    char *mapped_mem;
    char *pixels;

    int stride;

    bool lastcleared;
    bool pipon;
    bool osdon;

    char *osdbuffer;
    char *osdbuf_aligned;
    int   osdbufsize;
    int   osdbuf_revision;

    float last_speed;
    int   internal_offset;
    int   frame_at_speed_change;

    bool last_normal;
    int last_mask;
    eAlphaState alphaState;

    bool old_fb_ioctl;
    int  fb_dma_ioctl;
    bool color_key;
    bool decoder_flush;

    bool paused;

    VideoOutputIvtvPriv *priv;
};

#endif
