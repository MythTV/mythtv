// -*- Mode: c++ -*-

#ifndef MPEGRECORDER_H_
#define MPEGRECORDER_H_

#include "v4lrecorder.h"
#include "tspacket.h"
#include "mpegstreamdata.h"
#include "DeviceReadBuffer.h"

struct AVFormatContext;
struct AVPacket;

class MpegRecorder : public V4LRecorder,
                     public DeviceReaderCB
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
    void run(void);
    void Reset(void);

    void Pause(bool clear = true);
    bool PauseAndWait(int timeout = 100);

    bool IsRecording(void) { return recording; }

    bool Open(void);
    int GetVideoFd(void) { return chanfd; }

    // TSPacketListener
    bool ProcessTSPacket(const TSPacket &tspacket);

    // DeviceReaderCB
    virtual void ReaderPaused(int fd) { pauseWait.wakeAll(); }
    virtual void PriorityEvent(int fd) { }

  private:
    virtual void InitStreamData(void);
    void SetIntOption(RecordingProfile *profile, const QString &name);
    void SetStrOption(RecordingProfile *profile, const QString &name);

    bool OpenMpegFileAsInput(void);
    bool OpenV4L2DeviceAsInput(void);
    bool SetV4L2DeviceOptions(int chanfd);
    bool SetVideoCaptureFormat(int chanfd);
    bool SetLanguageMode(int chanfd);
    bool SetRecordingVolume(int chanfd);
    bool SetVBIOptions(int chanfd);
    uint GetFilteredStreamType(void) const;
    uint GetFilteredAudioSampleRate(void) const;
    uint GetFilteredAudioLayer(void) const;
    uint GetFilteredAudioBitRate(uint audio_layer) const;

    void RestartEncoding(void);
    bool StartEncoding(void);
    void StopEncoding(void);

    void SetBitrate(int bitrate, int maxbitrate, const QString & reason);
    void HandleResolutionChanges(void);

    virtual void FormatCC(uint code1, uint code2); // RecorderBase

    bool deviceIsMpegFile;
    int bufferSize;

    // Driver info
    QString  card;
    QString  driver;
    uint32_t version;
    bool     supports_sliced_vbi;

    // State
    mutable QMutex start_stop_encoding_lock;

    // Pausing state
    bool cleartimeonpause;

    // Encoding info
    int width, height;
    int bitrate, maxbitrate, streamtype, aspectratio;
    int audtype, audsamplerate, audbitratel1, audbitratel2, audbitratel3;
    int audvolume;
    unsigned int language; ///< 0 is Main Lang; 1 is SAP Lang; 2 is Dual
    unsigned int low_mpeg4avgbitrate;
    unsigned int low_mpeg4peakbitrate;
    unsigned int medium_mpeg4avgbitrate;
    unsigned int medium_mpeg4peakbitrate;
    unsigned int high_mpeg4avgbitrate;
    unsigned int high_mpeg4peakbitrate;

    // Input file descriptors
    int chanfd;
    int readfd;

    static const int   audRateL1[];
    static const int   audRateL2[];
    static const int   audRateL3[];
    static const char *streamType[];
    static const char *aspectRatio[];
    static const unsigned int kBuildBufferMaxSize;

    // Buffer device reads
    DeviceReadBuffer *_device_read_buffer;
};

#endif
