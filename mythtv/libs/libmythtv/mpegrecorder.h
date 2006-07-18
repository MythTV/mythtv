// -*- Mode: c++ -*-

#ifndef MPEGRECORDER_H_
#define MPEGRECORDER_H_

#include "recorderbase.h"

struct AVFormatContext;
struct AVPacket;

class MpegRecorder : public RecorderBase
{
  public:
    MpegRecorder(TVRec*);
   ~MpegRecorder();
    void TeardownAll(void);

    void SetOption(const QString &opt, int value);
    void SetOption(const QString &name, const QString &value);
    void SetVideoFilters(QString&) {}

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev, 
                               const QString &audiodev,
                               const QString &vbidev);

    void Initialize(void) {}
    void StartRecording(void);
    void StopRecording(void);
    void Reset(void);

    void Pause(bool clear = true);

    bool IsRecording(void) { return recording; }
    bool IsErrored(void) { return errored; }

    long long GetFramesWritten(void) { return framesWritten; }

    bool Open(void);
    int GetVideoFd(void) { return chanfd; }

    long long GetKeyframePosition(long long desired);

    void SetNextRecording(const ProgramInfo*, RingBuffer*);

  private:
    bool SetupRecording(void);
    void FinishRecording(void);
    void HandleKeyframe(void);
    void SavePositionMap(bool force);

    void ProcessData(unsigned char *buffer, int len);

    bool OpenMpegFileAsInput(void);
    bool OpenV4L2DeviceAsInput(void);
    bool SetIVTVDeviceOptions(int chanfd);
    bool SetV4L2DeviceOptions(int chanfd);

    void ResetForNewFile(void);

    bool deviceIsMpegFile;
    int bufferSize;

    // State
    bool recording;
    bool encoding;
    bool errored;

    // Pausing state
    bool cleartimeonpause;

    // Number of frames written
    long long framesWritten;

    // Encoding info
    int width, height;
    int bitrate, maxbitrate, streamtype, aspectratio;
    int audtype, audsamplerate, audbitratel1, audbitratel2;
    int audvolume;

    // Input file descriptors
    int chanfd;
    int readfd;

    // Keyframe tracking inforamtion
    int keyframedist;
    bool gopset;
    unsigned int leftovers;
    long long lastpackheaderpos;
    long long lastseqstart;
    long long numgops;

    // Position map support
    QMutex                     positionMapLock;
    QMap<long long, long long> positionMap;
    QMap<long long, long long> positionMapDelta;

    // buffer used for ...
    unsigned char *buildbuffer;
    unsigned int buildbuffersize;

    static const int   audRateL1[];
    static const int   audRateL2[];
    static const char *streamType[];
    static const char *aspectRatio[];
    static const unsigned int kBuildBufferMaxSize;
};
#endif
