#ifndef MPEGRECORDER_H_
#define MPEGRECORDER_H_

#include "recorderbase.h"

class MpegRecorder : public RecorderBase
{
  public:
    MpegRecorder();
   ~MpegRecorder();

    void SetEncodingOption(const QString &opt, int value);
    void ChangeDeinterlacer(int deint_mode);
    void SetVideoFilters(QString &filters);

    void Initialize(void);
    void StartRecording(void);
    void StopRecording(void);
    void Reset(void);

    void Pause(bool clear = true);
    void Unpause(void);
    bool GetPause(void);
    bool IsRecording(void);

    long long GetFramesWritten(void);

    int GetVideoFd(void);

    void TransitionToFile(const QString &lfilename);
    void TransitionToRing(void);

    long long GetKeyframePosition(long long desired);
    void GetBlankFrameMap(QMap<long long, int> &blank_frame_map);

  protected:
    static void *WriteThread(void *param);

    void doWriteThread(void);

  private:
    int SpawnChildren(void);
    void KillChildren(void);

    bool childrenLive;

    pthread_t write_tid;

    bool recording;
    bool encoding;

    bool paused;
    bool pausewritethread;
    bool actuallypaused;
    bool mainpaused;
    bool cleartimeonpause;

    long long framesWritten;

    int width, height;

    int chanfd;
    int readfd;
};

#endif
