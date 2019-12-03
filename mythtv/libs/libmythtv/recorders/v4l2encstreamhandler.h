// -*- Mode: c++ -*-

#ifndef _V4L2encStreamhandler_H_
#define _V4L2encStreamhandler_H_

#include <cstdint>
#include <vector>
using namespace std;

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
    ~V4L2encStreamHandler(void);

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
    int           m_desired_stream_type    {-1};
    int           m_stream_type            {-1};
    int           m_aspect_ratio           {-1};
    int           m_bitrate_mode           {V4L2_MPEG_VIDEO_BITRATE_MODE_VBR};
    int           m_bitrate                {-1};
    int           m_max_bitrate            {-1};
    int           m_audio_codec            {-1};
    int           m_audio_layer            {V4L2_MPEG_AUDIO_ENCODING_LAYER_1};
    int           m_audio_samplerate       {-1};
    int           m_audio_bitrateL1        {-1};
    int           m_audio_bitrateL2        {-1};
    int           m_audio_bitrateL3        {-1};
    int           m_audio_volume           {-1};
    int           m_lang_mode              {-1}; ///< 0 is Main Lang; 1 is SAP Lang; 2 is Dual
    uint          m_low_bitrate_mode       {V4L2_MPEG_VIDEO_BITRATE_MODE_VBR};
    uint          m_low_bitrate            {UINT_MAX};
    uint          m_low_peak_bitrate       {UINT_MAX};
    uint          m_medium_bitrate_mode    {V4L2_MPEG_VIDEO_BITRATE_MODE_VBR};
    uint          m_medium_bitrate         {UINT_MAX};
    uint          m_medium_peak_bitrate    {UINT_MAX};
    uint          m_high_bitrate_mode      {V4L2_MPEG_VIDEO_BITRATE_MODE_VBR};
    uint          m_high_bitrate           {UINT_MAX};
    uint          m_high_peak_bitrate      {UINT_MAX};

    static const int   s_audio_rateL1[];
    static const int   s_audio_rateL2[];
    static const int   s_audio_rateL3[];
    static const char *s_stream_types[];
    static const char *s_aspect_ratios[];

    int           m_fd                     {-1};
    int           m_audio_input;

    uint          m_width                  {UINT_MAX};
    uint          m_height                 {UINT_MAX};
    bool          m_has_lock               {false};
    uint          m_signal_strength        {UINT_MAX};

    V4L2util      m_v4l2;
    DeviceReadBuffer *m_drb                {nullptr};

    // VBI
    QString       m_vbi_device;

    QAtomicInt    m_streaming_cnt          {0};
    QMutex        m_stream_lock;

    bool          m_pause_encoding_allowed {true};
};

#endif // _V4L2encSTREAMHANDLER_H_
