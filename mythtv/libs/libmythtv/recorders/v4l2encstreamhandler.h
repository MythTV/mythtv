// -*- Mode: c++ -*-

#ifndef V4L2encStreamhandler_H
#define V4L2encStreamhandler_H

#include <cstdint>
#include <string>
#include <vector>

#include <QString>
#include <QAtomicInt>
#include <QMutex>
#include <QMap>

#include "streamhandler.h"
#include "v4l2util.h"

class DTVSignalMonitor;
class V4L2encChannel;

class V4L2encStreamHandler : public StreamHandler
{
    friend class V4L2encRecorder;
    friend class V4L2encSignalMonitor;

    enum constants {PACKET_SIZE = 188 * 32768};

  public:
    static V4L2encStreamHandler *Get(const QString &devname, int audioinput,
                                     int inputid);
    static void Return(V4L2encStreamHandler * & ref, int inputid);

  public:
    V4L2encStreamHandler(const QString & device, int audio_input, int inputid);
    ~V4L2encStreamHandler(void) override;

    void run(void) override; // MThread
#if 0
    void PriorityEvent(int fd) override; // DeviceReaderCB
#endif

    bool Configure(void);

    QString Driver(void) const { return m_v4l2.DriverName(); }
    int  GetStreamType(void);
    bool IsOpen(void) const { return m_fd >= 0 && m_v4l2.IsOpen(); }

    bool HasTuner(void) const { return m_v4l2.HasTuner(); }
    bool HasAudio(void) const { return m_v4l2.HasAudioSupport(); }
    bool HasSlicedVBI(void) const { return m_v4l2.HasSlicedVBI(); }

    bool HasPictureAttributes(void) const { return m_hasPictureAttributes; }
//    bool HasCap(uint32_t mask) const { return (mask & m_caps); }

    int  AvailCount(void) const { return m_drb ? m_drb->GetUsed() : 0; }
    bool StartEncoding(void);
    bool StopEncoding(void);
    void RestartEncoding(void);

    QString ErrorString(void) const { return m_error; }

  protected:
    bool Status(bool &failed, bool &failing);

    static QString RequestDescription(int request);

    bool SetOption(const QString &opt, int value);
    bool SetOption(const QString &opt, const QString &value);

    bool SetControl(int request, int value);
    bool SetVideoCaptureFormat(void);
    bool SetLanguageMode(void);
    bool SetRecordingVolume(void);

    bool HasLock(void);
    int  GetSignalStrength(void);
    bool GetResolution(int& width, int&height) const
        { return m_v4l2.GetResolution(width, height); }

    void SetBitrate(int bitrate, int maxbitrate, int bitratemode,
                    const QString & reason);
    bool SetBitrateForResolution(void);

  private:
    bool Open(void);
    void Close(void);
//    int Read(unsigned char *buf, uint count);
    bool ConfigureVBI(void);

    bool           m_failing               {false};
    QString        m_error;

    bool           m_hasTuner              {false};
    bool           m_hasPictureAttributes  {false};

    // for implementing Get & Return
    static QMutex                           s_handlers_lock;
    static QMap<QString, V4L2encStreamHandler*> s_handlers;
    static QMap<QString, uint>              s_handlers_refcnt;

    int           m_bufferSize             {1000 * (int)TSPacket::kSize};

    // Encoding info
    int           m_desiredStreamType      {-1};
    int           m_streamType             {-1};
    int           m_aspectRatio            {-1};
    int           m_bitrateMode            {V4L2_MPEG_VIDEO_BITRATE_MODE_VBR};
    int           m_bitrate                {-1};
    int           m_maxBitrate             {-1};
    int           m_audioCodec             {-1};
    int           m_audioLayer             {V4L2_MPEG_AUDIO_ENCODING_LAYER_1};
    int           m_audioSampleRate        {-1};
    int           m_audioBitrateL1         {-1};
    int           m_audioBitrateL2         {-1};
    int           m_audioBitrateL3         {-1};
    int           m_audioVolume            {-1};
    int           m_langMode               {-1}; ///< 0 is Main Lang; 1 is SAP Lang; 2 is Dual
    uint          m_lowBitrateMode         {V4L2_MPEG_VIDEO_BITRATE_MODE_VBR};
    uint          m_lowBitrate             {UINT_MAX};
    uint          m_lowPeakBitrate         {UINT_MAX};
    uint          m_mediumBitrateMode      {V4L2_MPEG_VIDEO_BITRATE_MODE_VBR};
    uint          m_mediumBitrate          {UINT_MAX};
    uint          m_mediumPeakBitrate      {UINT_MAX};
    uint          m_highBitrateMode        {V4L2_MPEG_VIDEO_BITRATE_MODE_VBR};
    uint          m_highBitrate            {UINT_MAX};
    uint          m_highPeakBitrate        {UINT_MAX};

    static const std::array<const int,14>         kAudioRateL1;
    static const std::array<const int,14>         kAudioRateL2;
    static const std::array<const int,14>         kAudioRateL3;
    static const std::array<const std::string,15> kStreamTypes;

    int           m_fd                     {-1};
    int           m_audioInput             {-1};

    uint          m_width                  {UINT_MAX};
    uint          m_height                 {UINT_MAX};
    uint          m_signalStrength         {UINT_MAX};

    V4L2util      m_v4l2;
    DeviceReadBuffer *m_drb                {nullptr};

    // VBI
    QString       m_vbiDevice;

    QAtomicInt    m_streamingCnt           {0};
    QMutex        m_streamLock;

    bool          m_pauseEncodingAllowed   {true};
};

#endif // V4L2encSTREAMHANDLER_H
