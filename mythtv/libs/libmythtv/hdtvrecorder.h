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
    void WaitForPause(void);
    bool IsRecording(void);

    long long GetFramesWritten(void);

    int GetVideoFd(void);

    long long GetKeyframePosition(long long desired);
    void GetBlankFrameMap(QMap<long long, int> &blank_frame_map);

  private:
    bool SetupRecording();
    void FinishRecording();

    int ProcessData(unsigned char *buffer, int len);
    void FindKeyframes(const unsigned char *buffer, 
		       int packet_start_pos,
		       int pkt_pos,
		       char adaptation_field_control,
		       bool payload_unit_start_indicator);

    bool recording;
    bool encoding;

    bool paused;
    bool mainpaused;
    bool cleartimeonpause;

    long long framesWritten;

    int width, height;

    int chanfd;

    int keyframedist;
    bool gopset;

    QMap<long long, long long> positionMap;

    long long prev_gop_save_pos;
    int firstgoppos;

    int pat_pid;
    int psip_pid;
    int video_pid;
    int audio_pid;
    int base_pid;

    int ts_packets;

    int lowest_video_pid;
    int video_pid_packets;
};

#endif
