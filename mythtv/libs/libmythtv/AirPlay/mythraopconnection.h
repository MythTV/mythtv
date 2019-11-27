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

#include "mythtvexp.h"

#include "mythnotification.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class QTcpSocket;
class QUdpSocket;
class QTimer;
class AudioOutput;
class ServerPool;
class _NetStream;

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
    int         GetDataPort() { return m_dataPort; }
    bool        HasAudio()    { return m_audio;    }
    static QMap<QString,QString> decodeDMAP(const QByteArray &dmap);
    static RSA *LoadKey(void);
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
    void     SendResendRequest(uint64_t timestamp,
                               uint16_t expected, uint16_t got);
    void     ExpireResendRequests(uint64_t timestamp);
    uint32_t decodeAudioPacket(uint8_t type, const QByteArray *buf,
                               QList<AudioData> *dest);
    int      ExpireAudio(uint64_t timestamp);
    void     ResetAudio(void);
    void     ProcessRequest(const QStringList &header,
                            const QByteArray &content);
    static void  FinishResponse(_NetStream *stream, QTcpSocket *socket,
                                QString &option, QString &cseq, QString &responseData);
    void     FinishAuthenticationResponse(_NetStream *stream, QTcpSocket *socket,
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
    static uint64_t NTPToLocal(uint32_t sec, uint32_t ticks);

    // incoming data packet
    static bool GetPacketType(const QByteArray &buf, uint8_t &type,
                          uint16_t &seq, uint64_t &timestamp);

    // utility functions
    int64_t     AudioCardLatency(void);
    static QStringList splitLines(const QByteArray &lines);
    static QString     stringFromSeconds(int timeInSeconds);
    uint64_t    framesToMs(uint64_t frames);

    // notification functions
    void        SendNotification(bool update = false);

    QTimer         *m_watchdogTimer       {nullptr};
    // comms socket
    QTcpSocket     *m_socket              {nullptr};
    _NetStream     *m_textStream          {nullptr};
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
    QMap<uint16_t,uint64_t> m_resends;
    // crypto
    QByteArray      m_AESIV;
    AES_KEY         m_aesKey;
    static RSA     *g_rsa;
    static QString  g_rsaLastError;
    // audio out
    AudioOutput    *m_audio               {nullptr};
    AVCodec        *m_codec               {nullptr};
    AVCodecContext *m_codeccontext        {nullptr};
    QList<int>      m_audioFormat;
    int             m_channels            {2};
    int             m_sampleSize          {16};
    int             m_frameRate           {44100};
    int             m_framesPerPacket     {352};
    QTimer         *m_dequeueAudioTimer   {nullptr};

    QMap<uint64_t, AudioPacket>  m_audioQueue;
    uint32_t        m_queueLength         {0};
    bool            m_streamingStarted    {false};
    bool            m_allowVolumeControl  {true};

    // packet index, increase after each resend packet request
    uint16_t        m_seqNum              {0};
    // audio/packet sync
    uint16_t        m_lastSequence        {0};
    uint64_t        m_lastTimestamp       {0};
    uint64_t        m_currentTimestamp    {0};
    uint16_t        m_nextSequence        {0};
    uint64_t        m_nextTimestamp       {0};
    int64_t         m_bufferLength        {0};
    uint64_t        m_timeLastSync        {0};
    int64_t         m_cardLatency         {-1};
    int64_t         m_adjustedLatency     {-1};
    bool            m_audioStarted        {false};

    // clock sync
    uint64_t        m_masterTimeStamp     {0};
    uint64_t        m_deviceTimeStamp     {0};
    uint64_t        m_networkLatency      {0};
                    // difference in ms between reference
    int64_t         m_clockSkew           {0};

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
    bool            m_firstsend           {false};
    bool            m_playbackStarted     {false};

  private slots:
    void ProcessAudio(void);
};

#endif // MYTHRAOPCONNECTION_H
