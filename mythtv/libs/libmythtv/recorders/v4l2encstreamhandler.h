// -*- Mode: c++ -*-

#ifndef _V4L2encStreamhandler_H_
#define _V4L2encStreamhandler_H_

#include <vector>
using namespace std;

#include <QString>
#include <QAtomicInt>
#include <QMutex>
#include <QMap>
#include <stdint.h>

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
    static V4L2encStreamHandler *Get(const QString &devicename, int audioinput);
    static void Return(V4L2encStreamHandler * & ref);

  public:
    V4L2encStreamHandler(const QString & path, int audio_input);
    ~V4L2encStreamHandler(void);

    virtual void run(void); // MThread
#if 0
    virtual void PriorityEvent(int fd); // DeviceReaderCB
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

    bool           m_failing;
    QString        m_error;

    bool           m_hasTuner;
    bool           m_hasPictureAttributes;

    // for implementing Get & Return
    static QMutex                           m_handlers_lock;
    static QMap<QString, V4L2encStreamHandler*> m_handlers;
    static QMap<QString, uint>              m_handlers_refcnt;

    int           m_bufferSize;

    // Encoding info
    int m_desired_stream_type, m_stream_type, m_aspect_ratio;
    int m_bitrate_mode, m_bitrate, m_max_bitrate;
    int m_audio_codec, m_audio_layer, m_audio_samplerate;
    int m_audio_bitrateL1, m_audio_bitrateL2, m_audio_bitrateL3;
    int m_audio_volume;
    int m_lang_mode; ///< 0 is Main Lang; 1 is SAP Lang; 2 is Dual
    uint m_low_bitrate_mode;
    uint m_low_bitrate;
    uint m_low_peak_bitrate;
    uint m_medium_bitrate_mode;
    uint m_medium_bitrate;
    uint m_medium_peak_bitrate;
    uint m_high_bitrate_mode;
    uint m_high_bitrate;
    uint m_high_peak_bitrate;

    static const int   m_audio_rateL1[];
    static const int   m_audio_rateL2[];
    static const int   m_audio_rateL3[];
    static const char *m_stream_types[];
    static const char *m_aspect_ratios[];

    int           m_fd;
    int           m_audio_input;

    uint          m_width;
    uint          m_height;
    bool          m_has_lock;
    uint          m_signal_strength;

    V4L2util      m_v4l2;
    DeviceReadBuffer *m_drb;

    // VBI
    QString       m_vbi_device;

    QAtomicInt    m_streaming_cnt;
    QMutex        m_stream_lock;

    bool          m_pause_encoding_allowed;
};

#endif // _V4L2encSTREAMHANDLER_H_
