#ifndef HDTVRECORDER_H_
#define HDTVRECORDER_H_

#include "recorderbase.h"

struct AVFormatContext;
struct AVPacket;

class HDTVRecorder : public RecorderBase
{
  public:
    HDTVRecorder();
   ~HDTVRecorder();

    void SetOption(const QString &opt, int value);
    void SetOption(const QString &name, const QString &value)
                       { RecorderBase::SetOption(name, value); }
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

    long long GetKeyframePosition(long long desired);
    void GetBlankFrameMap(QMap<long long, int> &blank_frame_map);

  private:
    bool SetupRecording();
    void FinishRecording();

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

    long long prev_gop_save_pos;
    int firstgoppos;
};

#endif
