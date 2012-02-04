#ifndef MYTHRAOPCONNECTION_H
#define MYTHRAOPCONNECTION_H

#include <QObject>
#include <QMap>
#include <QHash>
#include <QHostAddress>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/aes.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class QTextStream;
class QTcpSocket;
class QUdpSocket;
class QTimer;
class AudioOutput;

typedef QHash<QByteArray,QByteArray> RawHash;

class MythRAOPConnection : public QObject
{
    Q_OBJECT

  public:
    MythRAOPConnection(QObject *parent, QTcpSocket* socket, QByteArray id, int port);
   ~MythRAOPConnection();
    bool Init(void);
    QTcpSocket* GetSocket()   { return m_socket;   }
    int         GetDataPort() { return m_dataPort; }
    bool        HasAudio()    { return m_audio;    }

  public slots:
    void readClient(void);
    void udpDataReady(void);
    void timeout(void);
    void audioRetry(void);

  private:
    uint64_t FramesToMs(uint64_t timestamp);
    void    ProcessSyncPacket(const QByteArray &buf, uint64_t timenow);
    void    SendResendRequest(uint64_t timenow, uint16_t expected, uint16_t got);
    void    ExpireResendRequests(uint64_t timenow);
    int     ExpireAudio(uint64_t timestamp);
    void    ProcessAudio(uint64_t timenow);
    void    ResetAudio(void);
    void    ProcessRequest(const QList<QByteArray> &lines);
    void    StartResponse(QTextStream *stream);
    void    FinishResponse(QTextStream *stream, QTcpSocket *socket,
                           QByteArray &option, QByteArray &cseq);
    RawHash FindTags(const QList<QByteArray> &lines);
    static RSA* LoadKey(void);
    bool    CreateDecoder(void);
    void    DestroyDecoder(void);
    bool    OpenAudioDevice(void);
    void    CloseAudioDevice(void);
    void    StartAudioTimer(void);
    void    StopAudioTimer(void);

    QTimer         *m_watchdogTimer;
    // comms socket
    QTcpSocket     *m_socket;
    QTextStream    *m_textStream;
    QByteArray      m_hardwareId;
    // incoming audio
    QHostAddress    m_peerAddress;
    int             m_dataPort;
    QUdpSocket     *m_dataSocket;
    QUdpSocket     *m_clientControlSocket;
    int             m_clientControlPort;
    QMap<uint16_t,uint64_t> m_resends;
    // crypto
    QByteArray      m_AESIV;
    AES_KEY         m_aesKey;
    static RSA     *g_rsa;
    // audio out
    AudioOutput    *m_audio;
    AVCodec        *m_codec;
    AVCodecContext *m_codeccontext;
    QList<int>      m_audioFormat;
    int             m_sampleRate;
    QMap<uint64_t, int16_t*>  m_audioQueue;
    bool            m_allowVolumeControl;
    // audio/packet sync
    bool            m_seenPacket;
    int16_t         m_lastPacketSequence;
    uint64_t        m_lastPacketTimestamp;
    uint64_t        m_lastSyncTime;
    uint64_t        m_lastSyncTimestamp;
    uint64_t        m_lastLatency;
    uint64_t        m_latencyAudio;
    uint64_t        m_latencyQueued;
    uint64_t        m_latencyCounter;
    int64_t         m_avSync;
    // audio retry timer
    QTimer         *m_audioTimer;
};

#endif // MYTHRAOPCONNECTION_H
