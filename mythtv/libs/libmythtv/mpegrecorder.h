#ifndef MPEGRECORDER_H_
#define MPEGRECORDER_H_

#include "recorderbase.h"

struct AVFormatContext;
struct AVPacket;

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

  private:
    bool PacketHasHeader(unsigned char *buf, int len, unsigned int startcode);
    void ProcessData(unsigned char *buffer, int len);

    bool recording;
    bool encoding;

    bool paused;
    bool mainpaused;
    bool cleartimeonpause;

    long long framesWritten;

    int width, height;

    int chanfd;
    int readfd;

    AVFormatContext *ic;

    int keyframedist;
    bool gopset;

    QMap<long long, long long> positionMap;
};

#endif
