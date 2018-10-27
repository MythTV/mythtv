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
    explicit MpegRecorder(TVRec*);
   ~MpegRecorder();
    void TeardownAll(void);

    void SetOption(const QString &opt, int value) override; // DTVRecorder
    void SetOption(const QString &name, const QString &value) override; // DTVRecorder
    void SetVideoFilters(QString&) override {} // DTVRecorder

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev) override; // DTVRecorder

    void Initialize(void) override {} // DTVRecorder
    void run(void) override; // RecorderBase
    void Reset(void) override; // DTVRecorder

    void Pause(bool clear = true) override; // RecorderBase
    bool PauseAndWait(int timeout = 100) override; // RecorderBase

    bool IsRecording(void) override // RecorderBase
        { return recording; }

    bool Open(void);
    int GetVideoFd(void) override // DTVRecorder
        { return chanfd; }

    // TSPacketListener
    bool ProcessTSPacket(const TSPacket &tspacket) override; // DTVRecorder

    // DeviceReaderCB
    void ReaderPaused(int /*fd*/) override // DeviceReaderCB
        { pauseWait.wakeAll(); }
    void PriorityEvent(int /*fd*/) override { } //DeviceReaderCB

  private:
    void InitStreamData(void) override; // DTVRecorder
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

    bool RestartEncoding(void);
    bool StartEncoding(void);
    void StopEncoding(void);

    void SetBitrate(int bitrate, int maxbitrate, const QString & reason);
    bool HandleResolutionChanges(void);

    void FormatCC(uint code1, uint code2) override; // V4LRecorder

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
