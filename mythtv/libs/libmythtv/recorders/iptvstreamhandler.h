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
    IPTVStreamHandlerReadHelper(
        IPTVStreamHandler *p, QUdpSocket *s, uint stream);

  public slots:
    void ReadPending(void);

  private:
    IPTVStreamHandler *m_parent;
    QUdpSocket *m_socket;
    QHostAddress m_sender;
    uint m_stream;
};

class IPTVStreamHandlerWriteHelper : QObject
{
    Q_OBJECT

public:
    IPTVStreamHandlerWriteHelper(IPTVStreamHandler *);
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
    void timerEvent(QTimerEvent*);

private:
    IPTVStreamHandler *m_parent;
    int m_timer, m_timer_rtcp;
    uint m_last_sequence_number, m_last_timestamp, m_previous_last_sequence_number;
    int m_lost, m_lost_interval;
};

class IPTVStreamHandler : public StreamHandler
{
    friend class IPTVStreamHandlerReadHelper;
    friend class IPTVStreamHandlerWriteHelper;
  public:
    static IPTVStreamHandler *Get(const IPTVTuningData &tuning);
    static void Return(IPTVStreamHandler * & ref);

    virtual void AddListener(MPEGStreamData *data,
                             bool allow_section_reader = false,
                             bool needs_drb            = false,
                             QString output_file       = QString())
    {
        StreamHandler::AddListener(data, false, false, output_file);
    } // StreamHandler

  protected:
    IPTVStreamHandler(const IPTVTuningData &tuning);

    virtual void run(void); // MThread

  protected:
    IPTVTuningData m_tuning;
    QUdpSocket *m_sockets[IPTV_SOCKET_COUNT];
    IPTVStreamHandlerReadHelper *m_read_helpers[IPTV_SOCKET_COUNT];
    QHostAddress m_sender[IPTV_SOCKET_COUNT];
    IPTVStreamHandlerWriteHelper *m_write_helper;
    PacketBuffer *m_buffer;

    bool m_use_rtp_streaming;
    ushort m_rtsp_rtp_port, m_rtsp_rtcp_port;
    uint32_t m_rtsp_ssrc;
    QHostAddress m_rtcp_dest;

    // for implementing Get & Return
    static QMutex                            s_handlers_lock;
    static QMap<QString, IPTVStreamHandler*> s_handlers;
    static QMap<QString, uint>               s_handlers_refcnt;

private:
    void timerEvent(QTimerEvent*);

};

#endif // _IPTVSTREAMHANDLER_H_
