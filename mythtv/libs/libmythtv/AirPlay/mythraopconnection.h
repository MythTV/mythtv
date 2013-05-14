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

typedef QHash<QString,QString> RawHash;

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
   ~MythRAOPConnection();
    bool Init(void);
    QTcpSocket *GetSocket()   { return m_socket;   }
    int         GetDataPort() { return m_dataPort; }
    bool        HasAudio()    { return m_audio;    }
    static QMap<QString,QString> decodeDMAP(const QByteArray &dmap);
    static RSA *LoadKey(void);
    static QString RSALastError(void) { return g_rsaLastError; }

  private slots:
    void readClient(void);
    void udpDataReady(QByteArray buf, QHostAddress peer, quint16 port);
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
    void     FinishResponse(_NetStream *stream, QTcpSocket *socket,
                            QString &option, QString &cseq);
    void     FinishAuthenticationResponse(_NetStream *stream, QTcpSocket *socket,
                                          QString &cseq);

    RawHash  FindTags(const QStringList &lines);
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
    uint64_t NTPToLocal(uint32_t sec, uint32_t ticks);

    // incoming data packet
    bool    GetPacketType(const QByteArray &buf, uint8_t &type,
                          uint16_t &seq, uint64_t &timestamp);

    // utility functions
    int64_t     AudioCardLatency(void);
    QStringList splitLines(const QByteArray &lines);
    QString     stringFromSeconds(int seconds);
    uint64_t    framesToMs(uint64_t frames);

    QTimer         *m_watchdogTimer;
    // comms socket
    QTcpSocket     *m_socket;
    _NetStream     *m_textStream;
    QByteArray      m_hardwareId;
    QStringList     m_incomingHeaders;
    QByteArray      m_incomingContent;
    bool            m_incomingPartial;
    int32_t         m_incomingSize;
    QHostAddress    m_peerAddress;
    ServerPool     *m_dataSocket;
    int             m_dataPort;
    ServerPool     *m_clientControlSocket;
    int             m_clientControlPort;
    ServerPool     *m_clientTimingSocket;
    int             m_clientTimingPort;
    ServerPool     *m_eventServer;
    int             m_eventPort;
    QList<QTcpSocket *> m_eventClients;

    // incoming audio
    QMap<uint16_t,uint64_t> m_resends;
    // crypto
    QByteArray      m_AESIV;
    AES_KEY         m_aesKey;
    static RSA     *g_rsa;
    static QString  g_rsaLastError;
    // audio out
    AudioOutput    *m_audio;
    AVCodec        *m_codec;
    AVCodecContext *m_codeccontext;
    QList<int>      m_audioFormat;
    int             m_channels;
    int             m_sampleSize;
    int             m_frameRate;
    int             m_framesPerPacket;
    QTimer         *m_dequeueAudioTimer;

    QMap<uint64_t, AudioPacket>  m_audioQueue;
    uint32_t        m_queueLength;
    bool            m_streamingStarted;
    bool            m_allowVolumeControl;

    // packet index, increase after each resend packet request
    uint16_t        m_seqNum;
    // audio/packet sync
    uint16_t        m_lastSequence;
    uint64_t        m_lastTimestamp;
    uint64_t        m_currentTimestamp;
    uint16_t        m_nextSequence;
    uint64_t        m_nextTimestamp;
    int64_t         m_bufferLength;
    uint64_t        m_timeLastSync;
    int64_t         m_cardLatency;
    int64_t         m_adjustedLatency;
    bool            m_audioStarted;

    // clock sync
    uint64_t        m_masterTimeStamp;
    uint64_t        m_deviceTimeStamp;
    uint64_t        m_networkLatency;
    int64_t         m_clockSkew;       // difference in ms between reference

    // audio retry timer
    QTimer         *m_audioTimer;

    //Current Stream Info
    uint32_t        m_progressStart;
    uint32_t        m_progressCurrent;
    uint32_t        m_progressEnd;
    QByteArray      m_artwork;
    QByteArray      m_dmap;

    //Authentication
    QString         m_nonce;

  private slots:
    void ProcessAudio(void);
};

#endif // MYTHRAOPCONNECTION_H
