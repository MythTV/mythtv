// -*- Mode: c++ -*-

// System headers
#ifdef _WIN32
#  include <ws2tcpip.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
#endif

// Qt headers
#include <QUdpSocket>
#include <QByteArray>
#include <QHostInfo>

// MythTV headers
#include "iptvstreamhandler.h"
#include "rtppacketbuffer.h"
#include "udppacketbuffer.h"
#include "rtptsdatapacket.h"
#include "rtpdatapacket.h"
#include "rtpfecpacket.h"
#include "rtcpdatapacket.h"
#include "mythlogging.h"
#include "cetonrtsp.h"

#define LOC QString("IPTVSH(%1): ").arg(_device)

QMap<QString,IPTVStreamHandler*> IPTVStreamHandler::s_handlers;
QMap<QString,uint>               IPTVStreamHandler::s_handlers_refcnt;
QMutex                           IPTVStreamHandler::s_handlers_lock;

IPTVStreamHandler *IPTVStreamHandler::Get(const IPTVTuningData &tuning)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devkey = tuning.GetDeviceKey();

    QMap<QString,IPTVStreamHandler*>::iterator it = s_handlers.find(devkey);

    if (it == s_handlers.end())
    {
        IPTVStreamHandler *newhandler = new IPTVStreamHandler(tuning);
        newhandler->Start();
        s_handlers[devkey] = newhandler;
        s_handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("IPTVSH: Creating new stream handler %1 for %2")
            .arg(devkey).arg(tuning.GetDeviceName()));
    }
    else
    {
        s_handlers_refcnt[devkey]++;
        uint rcount = s_handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("IPTVSH: Using existing stream handler %1 for %2")
            .arg(devkey).arg(tuning.GetDeviceName()) +
            QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[devkey];
}

void IPTVStreamHandler::Return(IPTVStreamHandler * & ref)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devname = ref->_device;

    QMap<QString,uint>::iterator rit = s_handlers_refcnt.find(devname);
    if (rit == s_handlers_refcnt.end())
        return;

    LOG(VB_RECORD, LOG_INFO, QString("IPTVSH: Return(%1) has %2 handlers")
        .arg(devname).arg(*rit));

    if (*rit > 1)
    {
        ref = NULL;
        (*rit)--;
        return;
    }

    QMap<QString,IPTVStreamHandler*>::iterator it = s_handlers.find(devname);
    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("IPTVSH: Closing handler for %1")
                           .arg(devname));
        ref->Stop();
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("IPTVSH Error: Couldn't find handler for %1")
                .arg(devname));
    }

    s_handlers_refcnt.erase(rit);
    ref = NULL;
}

IPTVStreamHandler::IPTVStreamHandler(const IPTVTuningData &tuning) :
    StreamHandler(tuning.GetDeviceKey()),
    m_tuning(tuning),
    m_write_helper(NULL),
    m_buffer(NULL),
    m_rtsp_rtp_port(0),
    m_rtsp_rtcp_port(0),
    m_rtsp_ssrc(0)
{
    memset(m_sockets, 0, sizeof(m_sockets));
    memset(m_read_helpers, 0, sizeof(m_read_helpers));
    m_use_rtp_streaming = m_tuning.GetDataURL().scheme().toUpper() == "RTP";
}

void IPTVStreamHandler::run(void)
{
    RunProlog();

    LOG(VB_GENERAL, LOG_INFO, LOC + "run()");

    SetRunning(true, false, false);

    // TODO Error handling..

    // Setup
    CetonRTSP *rtsp = NULL;
    IPTVTuningData tuning = m_tuning;
    if (m_tuning.GetURL(0).scheme().toLower() == "rtsp")
    {
        rtsp = new CetonRTSP(m_tuning.GetURL(0));

        // Check RTSP capabilities
        QStringList options;
        if (!(rtsp->GetOptions(options)     && options.contains("DESCRIBE") &&
              options.contains("SETUP")     && options.contains("PLAY")     &&
              options.contains("TEARDOWN")))
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "RTSP interface did not support the necessary options");
            delete rtsp;
            SetRunning(false, false, false);
            RunEpilog();
            return;
        }

        if (!rtsp->Describe())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "RTSP Describe command failed");
            delete rtsp;
            SetRunning(false, false, false);
            RunEpilog();
            return;
        }

        m_use_rtp_streaming = true;

        QUrl urltuned = m_tuning.GetURL(0);
        urltuned.setScheme("rtp");
        urltuned.setPort(0);
        tuning = IPTVTuningData(urltuned.toString(), 0, IPTVTuningData::kNone,
                                urltuned.toString(), 0, "", 0);
    }

    bool error = false;
    int start_port = 0;
    for (uint i = 0; i < IPTV_SOCKET_COUNT; i++)
    {
        QUrl url = tuning.GetURL(i);
        if (url.port() < 0)
            continue;

        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("setting up url[%1]:%2").arg(i).arg(url.toString()));

        // always ensure we use consecutive port numbers
        int port = start_port ? start_port + 1 : url.port();
        QString host = url.host();
        QHostAddress dest_addr(host);

        if (!host.isEmpty() && dest_addr.isNull())
        {
            // address is a hostname, attempts to resolve it
            QHostInfo info = QHostInfo::fromName(host);
            QList<QHostAddress> list = info.addresses();

            if (list.isEmpty())
            {
                LOG(VB_RECORD, LOG_ERR, LOC +
                    QString("Can't resolve hostname:'%1'").arg(host));
            }
            else
            {
                for (int i=0; i < list.size(); i++)
                {
                    dest_addr = list[i];
                    if (list[i].protocol() == QAbstractSocket::IPv6Protocol)
                    {
                        // We prefer first IPv4
                        break;
                    }
                }
                LOG(VB_RECORD, LOG_DEBUG, LOC +
                    QString("resolved %1 as %2").arg(host).arg(dest_addr.toString()));
            }
        }
        bool ipv6 = dest_addr.protocol() == QAbstractSocket::IPv6Protocol;
        bool is_multicast = ipv6 ?
            dest_addr.isInSubnet(QHostAddress::parseSubnet("ff00::/8")) :
            (dest_addr.toIPv4Address() & 0xf0000000) == 0xe0000000;

        m_sockets[i] = new QUdpSocket();
        if (!is_multicast)
        {
            // this allow to filter incoming traffic, and make sure it's from
            // the requested server
            m_sender[i] = dest_addr;
        }
        m_read_helpers[i] = new IPTVStreamHandlerReadHelper(
            this, m_sockets[i], i);

        // we need to open the descriptor ourselves so we
        // can set some socket options
        int fd = socket(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0); // create IPv4 socket
        if (fd < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unable to create socket " + ENO);
            continue;
        }
        int buf_size = 2 * 1024 * max(tuning.GetBitrate(i)/1000, 500U);
        if (!tuning.GetBitrate(i))
            buf_size = 2 * 1024 * 1024;
        int err = setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                            (char *)&buf_size, sizeof(buf_size));
        if (err)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Increasing buffer size to %1 failed")
                .arg(buf_size) + ENO);
        }

        m_sockets[i]->setSocketDescriptor(
            fd, QAbstractSocket::UnconnectedState, QIODevice::ReadOnly);

        // we bind to destination address if it's a multicast address, or
        // the local ones otherwise
        if (!m_sockets[i]->bind(is_multicast ?
                                dest_addr :
                                (ipv6 ? QHostAddress::AnyIPv6 : QHostAddress::Any),
                                port))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Binding to port failed.");
            error = true;
        }
        else
        {
            start_port = m_sockets[i]->localPort();
        }

        if (is_multicast)
        {
            m_sockets[i]->joinMulticastGroup(dest_addr);
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Joining %1")
                .arg(dest_addr.toString()));
        }

        if (!is_multicast && rtsp && i == 1)
        {
            m_rtcp_dest = dest_addr;
        }
    }

    if (!error)
    {
        if (m_use_rtp_streaming)
            m_buffer = new RTPPacketBuffer(tuning.GetBitrate(0));
        else
            m_buffer = new UDPPacketBuffer(tuning.GetBitrate(0));
        m_write_helper =
            new IPTVStreamHandlerWriteHelper(this);
        m_write_helper->Start();
    }

    if (!error && rtsp)
    {
        // Start Streaming
        if (!rtsp->Setup(m_sockets[0]->localPort(), m_sockets[1]->localPort(),
                         m_rtsp_rtp_port, m_rtsp_rtcp_port, m_rtsp_ssrc) ||
            !rtsp->Play())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "Starting recording (RTP initialization failed). Aborting.");
            error = true;
        }
        if (m_rtsp_rtcp_port > 0)
        {
            m_write_helper->SendRTCPReport();
            m_write_helper->StartRTCPRR();
        }
    }

    if (!error)
    {
        // Enter event loop
        exec();
    }

    // Clean up
    for (uint i = 0; i < IPTV_SOCKET_COUNT; i++)
    {
        if (m_sockets[i])
        {
            delete m_sockets[i];
            m_sockets[i] = NULL;
            delete m_read_helpers[i];
            m_read_helpers[i] = NULL;
        }
    }
    delete m_buffer;
    m_buffer = NULL;
    delete m_write_helper;
    m_write_helper = NULL;

    if (rtsp)
    {
        rtsp->Teardown();
        delete rtsp;
    }

    SetRunning(false, false, false);
    RunEpilog();
}

IPTVStreamHandlerReadHelper::IPTVStreamHandlerReadHelper(
    IPTVStreamHandler *p, QUdpSocket *s, uint stream) :
    m_parent(p), m_socket(s), m_sender(p->m_sender[stream]),
    m_stream(stream)
{
    connect(m_socket, SIGNAL(readyRead()),
            this,     SLOT(ReadPending()));
}

#define LOC_WH QString("IPTVSH(%1): ").arg(m_parent->_device)

void IPTVStreamHandlerReadHelper::ReadPending(void)
{
    QHostAddress sender;
    quint16 senderPort;
    bool sender_null = m_sender.isNull();

    if (0 == m_stream)
    {
        while (m_socket->hasPendingDatagrams())
        {
            UDPPacket packet(m_parent->m_buffer->GetEmptyPacket());
            QByteArray &data = packet.GetDataReference();
            data.resize(m_socket->pendingDatagramSize());
            m_socket->readDatagram(data.data(), data.size(),
                                   &sender, &senderPort);
            if (sender_null || sender == m_sender)
            {
                m_parent->m_buffer->PushDataPacket(packet);
            }
            else
            {
                LOG(VB_RECORD, LOG_WARNING, LOC_WH +
                    QString("Received on socket(%1) %2 bytes from non expected "
                            "sender:%3 (expected:%4) ignoring")
                    .arg(m_stream).arg(data.size())
                    .arg(sender.toString()).arg(m_sender.toString()));
            }
        }
    }
    else
    {
        while (m_socket->hasPendingDatagrams())
        {
            UDPPacket packet(m_parent->m_buffer->GetEmptyPacket());
            QByteArray &data = packet.GetDataReference();
            data.resize(m_socket->pendingDatagramSize());
            m_socket->readDatagram(data.data(), data.size(),
                                   &sender, &senderPort);
            if (sender_null || sender == m_sender)
            {
                m_parent->m_buffer->PushFECPacket(packet, m_stream - 1);
            }
            else
            {
                LOG(VB_RECORD, LOG_WARNING, LOC_WH +
                    QString("Received on socket(%1) %2 bytes from non expected "
                            "sender:%3 (expected:%4) ignoring")
                    .arg(m_stream).arg(data.size())
                    .arg(sender.toString()).arg(m_sender.toString()));
            }
        }
    }
}

IPTVStreamHandlerWriteHelper::IPTVStreamHandlerWriteHelper(IPTVStreamHandler *p)
  : m_parent(p),                m_timer(0),             m_timer_rtcp(0),
    m_last_sequence_number(0),  m_last_timestamp(0),
    m_lost(0),                  m_lost_interval(0)
{
}

IPTVStreamHandlerWriteHelper::~IPTVStreamHandlerWriteHelper()
{
    if (m_timer)
    {
        killTimer(m_timer);
    }
    if (m_timer_rtcp)
    {
        killTimer(m_timer_rtcp);
    }
    m_timer = 0;
    m_timer_rtcp = 0;
    m_parent = NULL;
}

void IPTVStreamHandlerWriteHelper::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_timer_rtcp)
    {
        SendRTCPReport();
        return;
    }

    if (!m_parent->m_buffer->HasAvailablePacket())
        return;

    while (!m_parent->m_use_rtp_streaming)
    {
        UDPPacket packet(m_parent->m_buffer->PopDataPacket());

        if (packet.GetDataReference().isEmpty())
            break;

        int remainder = 0;
        {
            QMutexLocker locker(&m_parent->_listener_lock);
            QByteArray &data = packet.GetDataReference();
            IPTVStreamHandler::StreamDataList::const_iterator sit;
            sit = m_parent->_stream_data_list.begin();
            for (; sit != m_parent->_stream_data_list.end(); ++sit)
            {
                remainder = sit.key()->ProcessData(
                    reinterpret_cast<const unsigned char*>(data.data()),
                    data.size());
            }
        }

        if (remainder != 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC_WH +
                QString("data_length = %1 remainder = %2")
                .arg(packet.GetDataReference().size()).arg(remainder));
        }

        m_parent->m_buffer->FreePacket(packet);
    }

    while (m_parent->m_use_rtp_streaming)
    {
        RTPDataPacket packet(m_parent->m_buffer->PopDataPacket());

        if (!packet.IsValid())
            break;

        if (packet.GetPayloadType() == RTPDataPacket::kPayLoadTypeTS)
        {
            RTPTSDataPacket ts_packet(packet);

            if (!ts_packet.IsValid())
            {
                m_parent->m_buffer->FreePacket(packet);
                continue;
            }

            uint exp_seq_num = m_last_sequence_number + 1;
            uint seq_num = ts_packet.GetSequenceNumber();
            if (m_last_sequence_number &&
                ((exp_seq_num&0xFFFF) != (seq_num&0xFFFF)))
            {
                LOG(VB_RECORD, LOG_INFO, LOC_WH +
                    QString("Sequence number mismatch %1!=%2")
                    .arg(seq_num).arg(exp_seq_num));
                if (seq_num > exp_seq_num)
                {
                    m_lost_interval = seq_num - exp_seq_num;
                    m_lost += m_lost_interval;
                }
            }
            m_last_sequence_number = seq_num;
            m_last_timestamp = ts_packet.GetTimeStamp();
            LOG(VB_RECORD, LOG_DEBUG,
                QString("Processing RTP packet(seq:%1 ts:%2)")
                .arg(m_last_sequence_number).arg(m_last_timestamp));

            m_parent->_listener_lock.lock();

            int remainder = 0;
            IPTVStreamHandler::StreamDataList::const_iterator sit;
            sit = m_parent->_stream_data_list.begin();
            for (; sit != m_parent->_stream_data_list.end(); ++sit)
            {
                remainder = sit.key()->ProcessData(
                    ts_packet.GetTSData(), ts_packet.GetTSDataSize());
            }

            m_parent->_listener_lock.unlock();

            if (remainder != 0)
            {
                LOG(VB_RECORD, LOG_INFO, LOC_WH +
                    QString("data_length = %1 remainder = %2")
                    .arg(ts_packet.GetTSDataSize()).arg(remainder));
            }
        }
        m_parent->m_buffer->FreePacket(packet);
    }
}

void IPTVStreamHandlerWriteHelper::SendRTCPReport(void)
{
    if (m_parent->m_rtcp_dest.isNull())
    {
        // no point sending data if we don't know where to
        return;
    }
    int seq_delta = m_last_sequence_number - m_previous_last_sequence_number;
    RTCPDataPacket rtcp =
        RTCPDataPacket(m_last_timestamp, m_last_timestamp + RTCP_TIMER * 1000,
                       m_last_sequence_number, m_last_sequence_number + seq_delta,
                       m_lost, m_lost_interval, m_parent->m_rtsp_ssrc);
    QByteArray buf = rtcp.GetData();

    LOG(VB_RECORD, LOG_DEBUG, LOC_WH +
        QString("Sending RTCPReport to %1:%2")
        .arg(m_parent->m_rtcp_dest.toString())
        .arg(m_parent->m_rtsp_rtcp_port));
    m_parent->m_sockets[1]->writeDatagram(buf.constData(), buf.size(),
                                          m_parent->m_rtcp_dest, m_parent->m_rtsp_rtcp_port);
    m_previous_last_sequence_number = m_last_sequence_number;
}
