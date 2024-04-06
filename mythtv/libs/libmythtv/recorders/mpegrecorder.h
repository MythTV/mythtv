// -*- Mode: c++ -*-

#ifndef MPEGRECORDER_H_
#define MPEGRECORDER_H_

#include "DeviceReadBuffer.h"
#include "mpeg/mpegstreamdata.h"
#include "mpeg/tspacket.h"
#include "v4lrecorder.h"

struct AVFormatContext;
struct AVPacket;

class MpegRecorder : public V4LRecorder,
                     public DeviceReaderCB
{
  public:
    explicit MpegRecorder(TVRec*rec)
        : V4LRecorder(rec) {};
   ~MpegRecorder() override { TeardownAll(); }
    void TeardownAll(void);

    void SetOption(const QString &opt, int value) override; // DTVRecorder
    void SetOption(const QString &opt, const QString &value) override; // DTVRecorder
    void SetVideoFilters(QString &/*filters*/) override {} // DTVRecorder

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev) override; // DTVRecorder

    void Initialize(void) override {} // DTVRecorder
    void run(void) override; // RecorderBase
    void Reset(void) override; // DTVRecorder

    void Pause(bool clear = true) override; // RecorderBase
    bool PauseAndWait(std::chrono::milliseconds timeout = 100ms) override; // RecorderBase

    bool IsRecording(void) override // RecorderBase
        { return m_recording; }

    bool Open(void);
    int GetVideoFd(void) override // DTVRecorder
        { return m_chanfd; }

    // TSPacketListener
    bool ProcessTSPacket(const TSPacket &tspacket) override; // DTVRecorder

    // DeviceReaderCB
    void ReaderPaused(int /*fd*/) override // DeviceReaderCB
        { m_pauseWait.wakeAll(); }
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

    bool           m_deviceIsMpegFile         {false};
    int            m_bufferSize               {0};

    // Driver info
    QString        m_card;
    QString        m_driver;
    uint32_t       m_version                  {0};
    bool           m_supportsSlicedVbi        {false};

    // State
    mutable QRecursiveMutex m_startStopEncodingLock;

    // Pausing state
    bool           m_clearTimeOnPause         {false};

    // Encoding info
    int            m_width                    {720};
    int            m_height                   {480};
    int            m_bitrate                  {4500};
    int            m_maxBitrate               {6000};
    int            m_streamType               {0};
    int            m_aspectRatio              {2};
    int            m_audType                  {2};
    int            m_audSampleRate            {48000};
    int            m_audBitrateL1             {14};
    int            m_audBitrateL2             {14};
    int            m_audBitrateL3             {10};
    int            m_audVolume                {80};
                   /// 0 is Main Lang; 1 is SAP Lang; 2 is Dual
    unsigned int   m_language                 {0};
    unsigned int   m_lowMpeg4AvgBitrate       { 4500};
    unsigned int   m_lowMpeg4PeakBitrate      { 6000};
    unsigned int   m_mediumMpeg4AvgBitrate   { 9000};
    unsigned int   m_mediumMpeg4PeakBitrate  {13500};
    unsigned int   m_highMpeg4AvgBitrate     {13500};
    unsigned int   m_highMpeg4PeakBitrate    {20200};

    // Input file descriptors
    int            m_chanfd                   {-1};
    int            m_readfd                   {-1};

    static const std::array<const int,14>         kAudRateL1;
    static const std::array<const int,14>         kAudRateL2;
    static const std::array<const int,14>         kAudRateL3;
    static const std::array<const std::string,15> kStreamType;
    static const std::array<const std::string,4>  kAspectRatio;
    static const unsigned int kBuildBufferMaxSize;

    // Buffer device reads
    DeviceReadBuffer *m_deviceReadBuffer      {nullptr};
};

#endif
