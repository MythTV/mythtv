#ifndef DVBRECORDER_H_
#define DVBRECORDER_H_

#include <vector>
#include "recorderbase.h"

class DVBChannel;
struct AVFormatContext;
struct AVPacket;

/* Grabs the video/audio MPEG data from a DVB (a digital TV standard) card. */

class DVBRecorder : public RecorderBase
{
  public:
    DVBRecorder(const DVBChannel* dvbchannel);
   ~DVBRecorder();

  // Generic MPEG
  public:
    void SetOption(const QString &name, int value);
    void SetOption(const QString &name, const QString &value)
                                       { RecorderBase::SetOption(name, value); }
    void SetPID(const vector<int>& some_pids);
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

    AVFormatContext *ic;

    int keyframedist;
    bool gopset;

    QMap<long long, long long> positionMap;

  // DVB-specific
  protected:
    bool Open();
    void Close();

    bool isopen;
    int dvr_fd; // see dvbchannel.h
    int cardnum; // dito
    bool was_paused;

    vector<int> pid;
    bool pid_changed;
    const DVBChannel* channel;
};

#endif
