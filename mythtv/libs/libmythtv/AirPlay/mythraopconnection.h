#ifndef MYTHRAOPCONNECTION_H
#define MYTHRAOPCONNECTION_H

#include <QObject>
#include <QMap>
#include <QHash>
#include <QHostAddress>
#include <QStringList>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/aes.h>
#include <openssl/err.h>

#include "libmythtv/mythtvexp.h"

#include "libmythui/mythnotification.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class QTcpSocket;
class QUdpSocket;
class QTimer;
class AudioOutput;
class ServerPool;
class RaopNetStream;

using RawHash = QHash<QString,QString>;

struct AudioData
{
    uint8_t    *data;
    int32_t     length;
    int32_t     frames;
};

struct AudioPacket
{
    uint16_t          seq;
    QList<AudioData> *data;
};

class MTV_PUBLIC MythRAOPConnection : public QObject
{
    Q_OBJECT

    friend class MythRAOPDevice;

  public:
    MythRAOPConnection(QObject *parent, QTcpSocket *socket, QByteArray id,
                       int port);
   ~MythRAOPConnection() override;
    bool Init(void);
    QTcpSocket *GetSocket()   { return m_socket;   }
    int         GetDataPort() const { return m_dataPort; }
    bool        HasAudio()    { return m_audio;    }
    static QMap<QString,QString> decodeDMAP(const QByteArray &dmap);
    static bool LoadKey(void);
    static QString RSALastError(void) { return g_rsaLastError; }

  private slots:
    void readClient(void);
    void udpDataReady(QByteArray buf, const QHostAddress& peer, quint16 port);
    void timeout(void);
    void audioRetry(void);
    void newEventClient(QTcpSocket *client);
    void deleteEventClient();

  private:
    void     ProcessSync(const QByteArray &buf);
    void     SendResendRequest(std::chrono::milliseconds timestamp,
                               uint16_t expected, uint16_t got);
    void     ExpireResendRequests(std::chrono::milliseconds timestamp);
    uint32_t decodeAudioPacket(uint8_t type, const QByteArray *buf,
                               QList<AudioData> *dest);
    int      ExpireAudio(std::chrono::milliseconds timestamp);
    void     ResetAudio(void);
    void     ProcessRequest(const QStringList &header,
                            const QByteArray &content);
    static void  FinishResponse(RaopNetStream *stream, QTcpSocket *socket,
                                QString &option, QString &cseq, QString &responseData);
    void     FinishAuthenticationResponse(RaopNetStream *stream, QTcpSocket *socket,
                                          QString &cseq);

    static RawHash  FindTags(const QStringList &lines);
    bool     CreateDecoder(void);
    void     DestroyDecoder(void);
    bool     OpenAudioDevice(void);
    void     CloseAudioDevice(void);
    void     StartAudioTimer(void);
    void     StopAudioTimer(void);
    void     CleanUp(void);

    // time sync
    void     SendTimeRequest(void);
    void     ProcessTimeResponse(const QByteArray &buf);
    static std::chrono::milliseconds NTPToLocal(uint32_t sec, uint32_t ticks);
    static void microsecondsToNTP(std::chrono::microseconds usec, uint32_t &ntpSec, uint32_t &ntpTicks);
    static void timevalToNTP(timeval t, uint32_t &ntpSec, uint32_t &ntpTicks);

    // incoming data packet
    static bool GetPacketType(const QByteArray &buf, uint8_t &type,
                          uint16_t &seq, uint64_t &timestamp);

    // utility functions
    std::chrono::milliseconds AudioCardLatency(void);
    static QStringList splitLines(const QByteArray &lines);
    static QString     stringFromSeconds(int timeInSeconds);
    std::chrono::milliseconds framesToMs(uint64_t frames) const;
    uint64_t MsToFrame(std::chrono::milliseconds millis) const;

    // notification functions
    void        SendNotification(bool update = false);

    QTimer         *m_watchdogTimer       {nullptr};
    // comms socket
    QTcpSocket     *m_socket              {nullptr};
    RaopNetStream  *m_textStream          {nullptr};
    QByteArray      m_hardwareId;
    QStringList     m_incomingHeaders;
    QByteArray      m_incomingContent;
    bool            m_incomingPartial     {false};
    int32_t         m_incomingSize        {0};
    QHostAddress    m_peerAddress;
    ServerPool     *m_dataSocket          {nullptr};
    int             m_dataPort;
    ServerPool     *m_clientControlSocket {nullptr};
    int             m_clientControlPort   {0};
    ServerPool     *m_clientTimingSocket  {nullptr};
    int             m_clientTimingPort    {0};
    ServerPool     *m_eventServer         {nullptr};
    int             m_eventPort           {-1};
    QList<QTcpSocket *> m_eventClients;

    // incoming audio
    QMap<uint16_t,std::chrono::milliseconds> m_resends;
    // crypto
    QByteArray      m_aesIV;
    static EVP_PKEY      *g_devPrivKey;
    std::vector<uint8_t>  m_sessionKey;
#if OPENSSL_VERSION_NUMBER < 0x030000000L
    const EVP_CIPHER     *m_cipher        {nullptr};
#else
    EVP_CIPHER           *m_cipher        {nullptr};
#endif
    EVP_CIPHER_CTX       *m_cctx          {nullptr};
    static QString  g_rsaLastError;
    // audio out
    AudioOutput    *m_audio               {nullptr};
    const AVCodec  *m_codec               {nullptr};
    AVCodecContext *m_codecContext        {nullptr};

    QList<int>      m_audioFormat;
    //< For Apple Lossless streams, this data is passed as a series of
    //  12 integers.  Note: this is the format of the stream between
    //  sender and receiver, not the format of the file being played.
    //  An MP3 file is sent as an ALAC stream.
    //
    //   0: stream number (matches a=rtpmap:XX)
    //   1: frameLength - default frames per packet
    //   2: compatibleVersion  (must be 0)
    //   3: bitDepth           (max 32)
    //   4: pb                 (should be 40)
    //   5: mb                 (should be 10)
    //   6: kb                 (should be 14)
    //   7: numChannels
    //   8: maxRun             (unused, set to 255)
    //   9: maxFrameBytes      (0 is unknown)
    //  10: avgBitRate         (0 is unknown)
    //  11: sampleRate
    //
    //  Example:
    //  a=fmtp:96 352 0 16 40 10 14 2 255 0 0 44100
    //
    //  See https://github.com/macosforge/alac/blob/c38887c5c5e64a4b31108733bd79ca9b2496d987/ALACMagicCookieDescription.txt#L48

    int             m_channels            {2};
    int             m_sampleSize          {16};
    int             m_frameRate           {44100};
    int             m_framesPerPacket     {352};
    QTimer         *m_dequeueAudioTimer   {nullptr};

    QMap<std::chrono::milliseconds, AudioPacket>  m_audioQueue;
    uint32_t        m_queueLength         {0};
    bool            m_streamingStarted    {false};
    bool            m_allowVolumeControl  {true};

    // packet index, increase after each resend packet request
    uint16_t        m_seqNum              {0};
    // audio/packet sync
    uint16_t        m_lastSequence        {0};
    std::chrono::milliseconds  m_lastTimestamp       {0ms};
    std::chrono::milliseconds  m_currentTimestamp    {0ms};
    uint16_t        m_nextSequence        {0};
    std::chrono::milliseconds  m_nextTimestamp       {0ms};
    std::chrono::milliseconds  m_bufferLength        {0ms};
    std::chrono::milliseconds  m_timeLastSync        {0ms};
    std::chrono::milliseconds  m_cardLatency         {-1ms};
    std::chrono::milliseconds  m_adjustedLatency     {-1ms};
    bool            m_audioStarted        {false};

    // clock sync
    std::chrono::milliseconds  m_networkLatency      {0ms};
    // Difference in ms between reference. This value will typically
    // be huge (~50 years)and meaningless as Apple products seem to
    // only send uptimes, not wall times.
    std::chrono::milliseconds  m_clockSkew           {0ms};

    // audio retry timer
    QTimer         *m_audioTimer          {nullptr};

    //Current Stream Info
    uint32_t        m_progressStart       {0};
    uint32_t        m_progressCurrent     {0};
    uint32_t        m_progressEnd         {0};
    QByteArray      m_artwork;
    DMAP            m_dmap;

    //Authentication
    QString         m_nonce;

    // Notification Center registration Id
    int             m_id;
    bool            m_firstSend           {false};
    bool            m_playbackStarted     {false};

  private slots:
    void ProcessAudio(void);
};

#endif // MYTHRAOPCONNECTION_H
