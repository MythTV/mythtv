// -*- Mode: c++ -*-

#ifndef IPTVSTREAMHANDLER_H
#define IPTVSTREAMHANDLER_H

#include <vector>

#include <QHostAddress>
#include <QUdpSocket>
#include <QString>
#include <QMutex>
#include <QMap>

#include <QNetworkAccessManager>

#include "channelutil.h"
#include "streamhandler.h"

static constexpr size_t IPTV_SOCKET_COUNT   { 3 };
static constexpr std::chrono::milliseconds RTCP_TIMER { 10s };

class IPTVStreamHandler;
class DTVSignalMonitor;
class MPEGStreamData;
class PacketBuffer;
class IPTVChannel;

class IPTVStreamHandlerReadHelper : public QObject
{
    Q_OBJECT

  public:
    IPTVStreamHandlerReadHelper(IPTVStreamHandler *p, QUdpSocket *s, uint stream);

  public slots:
    void ReadPending(void);

  private:
    IPTVStreamHandler *m_parent {nullptr};
    QUdpSocket        *m_socket {nullptr};
    QHostAddress       m_sender;
    uint               m_stream;
};

class IPTVStreamHandlerWriteHelper : QObject
{
    Q_OBJECT

public:
    explicit IPTVStreamHandlerWriteHelper(IPTVStreamHandler *p)
        : m_parent(p) {}
    ~IPTVStreamHandlerWriteHelper() override;

    void Start(void)
    {
        m_timer = startTimer(200ms);
    }
    void StartRTCPRR(void)
    {
        m_timerRtcp = startTimer(RTCP_TIMER);
    }

    void SendRTCPReport(void);

private:
    void timerEvent(QTimerEvent *event) override; // QObject

private:
    IPTVStreamHandler *m_parent                        {nullptr};
    int                m_timer                         {0};
    int                m_timerRtcp                     {0};
    uint               m_lastSequenceNumber            {0};
    uint               m_lastTimestamp                 {0};
    uint               m_previousLastSequenceNumber    {0};
    int                m_lost                          {0};
    int                m_lostInterval                  {0};
};

class IPTVStreamHandler : public StreamHandler
{
    friend class IPTVStreamHandlerReadHelper;
    friend class IPTVStreamHandlerWriteHelper;
  public:
    static IPTVStreamHandler *Get(const IPTVTuningData &tuning, int inputid);
    static void Return(IPTVStreamHandler * & ref, int inputid);

    void AddListener(MPEGStreamData *data,
                     bool /*allow_section_reader*/ = false,
                     bool /*needs_drb*/            = false,
                     const QString& output_file    = QString()) override // StreamHandler
    {
        // Force allow_section_reader and needs_buffering to false;
        StreamHandler::AddListener(data, false, false, output_file);
    }

  protected:
    explicit IPTVStreamHandler(const IPTVTuningData &tuning, int inputid);

    void run(void) override; // MThread

  protected:
    IPTVTuningData                m_tuning;
    std::array<QUdpSocket*,IPTV_SOCKET_COUNT>                  m_sockets {};
    std::array<IPTVStreamHandlerReadHelper*,IPTV_SOCKET_COUNT> m_readHelpers {};
    std::array<QHostAddress,IPTV_SOCKET_COUNT>                 m_sender;
    IPTVStreamHandlerWriteHelper *m_writeHelper       {nullptr};
    PacketBuffer                 *m_buffer            {nullptr};

    bool                          m_useRtpStreaming;
    ushort                        m_rtspRtpPort       {0};
    ushort                        m_rtspRtcpPort      {0};
    uint32_t                      m_rtspSsrc          {0};
    QHostAddress                  m_rtcpDest;

    // for implementing Get & Return
    static QMutex                            s_iptvhandlers_lock;
    static QMap<QString, IPTVStreamHandler*> s_iptvhandlers;
    static QMap<QString, uint>               s_iptvhandlers_refcnt;

private:
    void timerEvent(QTimerEvent*);

};

#endif // IPTVSTREAMHANDLER_H
