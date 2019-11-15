// -*- Mode: c++ -*-

#ifndef _IPTVSTREAMHANDLER_H_
#define _IPTVSTREAMHANDLER_H_

#include <vector>
using namespace std;

#include <QHostAddress>
#include <QUdpSocket>
#include <QString>
#include <QMutex>
#include <QMap>

#include <QNetworkAccessManager>
#include <QtNetwork>

#include "channelutil.h"
#include "streamhandler.h"

#define IPTV_SOCKET_COUNT   3
#define RTCP_TIMER          10

class IPTVStreamHandler;
class DTVSignalMonitor;
class MPEGStreamData;
class PacketBuffer;
class IPTVChannel;

class IPTVStreamHandlerReadHelper : QObject
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
    ~IPTVStreamHandlerWriteHelper();

    void Start(void)
    {
        m_timer = startTimer(200);
    }
    void StartRTCPRR(void)
    {
        m_timer_rtcp = startTimer(RTCP_TIMER * 1000);
    }

    void SendRTCPReport(void);

private:
    void timerEvent(QTimerEvent*) override; // QObject

private:
    IPTVStreamHandler *m_parent                        {nullptr};
    int                m_timer                         {0};
    int                m_timer_rtcp                    {0};
    uint               m_last_sequence_number          {0};
    uint               m_last_timestamp                {0};
    uint               m_previous_last_sequence_number {0};
    int                m_lost                          {0};
    int                m_lost_interval                 {0};
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
                     QString output_file           = QString()) override // StreamHandler
    {
        // Force allow_section_reader and needs_buffering to false;
        StreamHandler::AddListener(data, false, false, output_file);
    }

  protected:
    explicit IPTVStreamHandler(const IPTVTuningData &tuning, int inputid);

    void run(void) override; // MThread

  protected:
    IPTVTuningData                m_tuning;
    QUdpSocket                   *m_sockets[IPTV_SOCKET_COUNT] {};
    IPTVStreamHandlerReadHelper  *m_read_helpers[IPTV_SOCKET_COUNT] {};
    QHostAddress                  m_sender[IPTV_SOCKET_COUNT];
    IPTVStreamHandlerWriteHelper *m_write_helper      {nullptr};
    PacketBuffer                 *m_buffer            {nullptr};

    bool                          m_use_rtp_streaming;
    ushort                        m_rtsp_rtp_port     {0};
    ushort                        m_rtsp_rtcp_port    {0};
    uint32_t                      m_rtsp_ssrc         {0};
    QHostAddress                  m_rtcp_dest;

    // for implementing Get & Return
    static QMutex                            s_iptvhandlers_lock;
    static QMap<QString, IPTVStreamHandler*> s_iptvhandlers;
    static QMap<QString, uint>               s_iptvhandlers_refcnt;

private:
    void timerEvent(QTimerEvent*);

};

#endif // _IPTVSTREAMHANDLER_H_
