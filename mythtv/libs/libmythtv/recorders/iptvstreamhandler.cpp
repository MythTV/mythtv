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
#include "libmythbase/mythlogging.h"

#include "cetonrtsp.h"
#include "iptvstreamhandler.h"
#include "rtp/rtcpdatapacket.h"
#include "rtp/rtpdatapacket.h"
#include "rtp/rtpfecpacket.h"
#include "rtp/rtppacketbuffer.h"
#include "rtp/rtptsdatapacket.h"
#include "rtp/udppacketbuffer.h"

#define LOC QString("IPTVSH[%1](%2): ").arg(m_inputId).arg(m_device)

QMap<QString,IPTVStreamHandler*> IPTVStreamHandler::s_iptvhandlers;
QMap<QString,uint>               IPTVStreamHandler::s_iptvhandlers_refcnt;
QMutex                           IPTVStreamHandler::s_iptvhandlers_lock;

IPTVStreamHandler *IPTVStreamHandler::Get(const IPTVTuningData &tuning,
                                          int inputid)
{
    QMutexLocker locker(&s_iptvhandlers_lock);

    QString devkey = tuning.GetDeviceKey();

    QMap<QString,IPTVStreamHandler*>::iterator it = s_iptvhandlers.find(devkey);

    if (it == s_iptvhandlers.end())
    {
        auto *newhandler = new IPTVStreamHandler(tuning, inputid);
        newhandler->Start();
        s_iptvhandlers[devkey] = newhandler;
        s_iptvhandlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("IPTVSH[%1]: Creating new stream handler %2 for %3")
            .arg(QString::number(inputid), devkey, tuning.GetDeviceName()));
    }
    else
    {
        s_iptvhandlers_refcnt[devkey]++;
        uint rcount = s_iptvhandlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("IPTVSH[%1]: Using existing stream handler %2 for %3")
            .arg(QString::number(inputid), devkey, tuning.GetDeviceName()) +
            QString(" (%1 in use)").arg(rcount));
    }

    return s_iptvhandlers[devkey];
}

void IPTVStreamHandler::Return(IPTVStreamHandler * & ref, int inputid)
{
    QMutexLocker locker(&s_iptvhandlers_lock);

    QString devname = ref->m_device;

    QMap<QString,uint>::iterator rit = s_iptvhandlers_refcnt.find(devname);
    if (rit == s_iptvhandlers_refcnt.end())
        return;

    LOG(VB_RECORD, LOG_INFO, QString("IPTVSH[%1]: Return(%2) has %3 handlers")
        .arg(QString::number(inputid), devname).arg(*rit));

    if (*rit > 1)
    {
        ref = nullptr;
        (*rit)--;
        return;
    }

    QMap<QString,IPTVStreamHandler*>::iterator it = s_iptvhandlers.find(devname);
    if ((it != s_iptvhandlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("IPTVSH[%1]: Closing handler for %2")
            .arg(QString::number(inputid), devname));
        ref->Stop();
        delete *it;
        s_iptvhandlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("IPTVSH[%1] Error: Couldn't find handler for %2")
            .arg(QString::number(inputid), devname));
    }

    s_iptvhandlers_refcnt.erase(rit);
    ref = nullptr;
}

IPTVStreamHandler::IPTVStreamHandler(const IPTVTuningData &tuning, int inputid)
    : StreamHandler(tuning.GetDeviceKey(), inputid)
    , m_tuning(tuning)
    , m_useRtpStreaming(m_tuning.IsRTP())
{
}

void IPTVStreamHandler::run(void)
{
    RunProlog();

    LOG(VB_GENERAL, LOG_INFO, LOC + "run()");

    SetRunning(true, false, false);

    // TODO Error handling..

    // Setup
    CetonRTSP *rtsp = nullptr;
    IPTVTuningData tuning = m_tuning;
    if(m_tuning.IsRTSP())
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

        m_useRtpStreaming = true;

        QUrl urltuned = m_tuning.GetURL(0);
        urltuned.setScheme("rtp");
        urltuned.setPort(0);
        tuning = IPTVTuningData(urltuned.toString(), 0, IPTVTuningData::kNone,
                                urltuned.toString(), 0, "", 0);
    }

    bool error = false;

    int start_port = 0;
    for (size_t i = 0; i < IPTV_SOCKET_COUNT; i++)
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
                for (const auto & addr : std::as_const(list))
                {
                    dest_addr = addr;
                    if (addr.protocol() == QAbstractSocket::IPv6Protocol)
                    {
                        // We prefer first IPv4
                        break;
                    }
                }
                LOG(VB_RECORD, LOG_DEBUG, LOC +
                    QString("resolved %1 as %2").arg(host, dest_addr.toString()));
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
        m_readHelpers[i] = new IPTVStreamHandlerReadHelper(this,m_sockets[i],i);

        // we need to open the descriptor ourselves so we
        // can set some socket options
        int fd = socket(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0); // create IPv4 socket
        if (fd < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unable to create socket " + ENO);
            continue;
        }
        int buf_size = 2 * 1024 * std::max(tuning.GetBitrate(i)/1000, 500U);
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
        QHostAddress a {QHostAddress::Any};
        if (is_multicast)
            a = dest_addr;
        else if (ipv6)
            a = QHostAddress::AnyIPv6;
        if (!m_sockets[i]->bind(a, port))
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
            m_rtcpDest = dest_addr;
        }
    }

    if (!error)
    {
        if (m_tuning.IsRTP() || m_tuning.IsRTSP())
            m_buffer = new RTPPacketBuffer(tuning.GetBitrate(0));
        else
            m_buffer = new UDPPacketBuffer(tuning.GetBitrate(0));
        m_writeHelper = new IPTVStreamHandlerWriteHelper(this);
        m_writeHelper->Start();
    }

    if (!error && rtsp)
    {
        // Start Streaming
        if (!rtsp->Setup(m_sockets[0]->localPort(), m_sockets[1]->localPort(),
                         m_rtspRtpPort, m_rtspRtcpPort, m_rtspSsrc) ||
            !rtsp->Play())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "Starting recording (RTP initialization failed). Aborting.");
            error = true;
        }
        if (m_rtspRtcpPort > 0)
        {
            m_writeHelper->SendRTCPReport();
            m_writeHelper->StartRTCPRR();
        }
    }

    if (!error)
    {
        // Enter event loop
        exec();
    }

    // Clean up
    for (size_t i = 0; i < IPTV_SOCKET_COUNT; i++)
    {
        if (m_sockets[i])
        {
            delete m_sockets[i];
            m_sockets[i] = nullptr;
            delete m_readHelpers[i];
            m_readHelpers[i] = nullptr;
        }
    }
    delete m_buffer;
    m_buffer = nullptr;
    delete m_writeHelper;
    m_writeHelper = nullptr;

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
    connect(m_socket, &QIODevice::readyRead,
            this,     &IPTVStreamHandlerReadHelper::ReadPending);
}

#define LOC_WH QString("IPTVSH(%1): ").arg(m_parent->m_device)

void IPTVStreamHandlerReadHelper::ReadPending(void)
{
    QHostAddress sender;
    quint16 senderPort = 0;
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
                    .arg(sender.toString(), m_sender.toString()));
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
                    .arg(sender.toString(), m_sender.toString()));
            }
        }
    }
}

IPTVStreamHandlerWriteHelper::~IPTVStreamHandlerWriteHelper()
{
    if (m_timer)
    {
        killTimer(m_timer);
    }
    if (m_timerRtcp)
    {
        killTimer(m_timerRtcp);
    }
    m_timer = 0;
    m_timerRtcp = 0;
    m_parent = nullptr;
}

void IPTVStreamHandlerWriteHelper::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_timerRtcp)
    {
        SendRTCPReport();
        return;
    }

    if (!m_parent->m_buffer->HasAvailablePacket())
        return;

    while (!m_parent->m_useRtpStreaming)
    {
        UDPPacket packet(m_parent->m_buffer->PopDataPacket());

        if (packet.GetDataReference().isEmpty())
            break;

        int remainder = 0;
        {
            QMutexLocker locker(&m_parent->m_listenerLock);
            QByteArray &data = packet.GetDataReference();
            for (auto sit = m_parent->m_streamDataList.cbegin();
                 sit != m_parent->m_streamDataList.cend(); ++sit)
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

    while (m_parent->m_useRtpStreaming)
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

            uint exp_seq_num = m_lastSequenceNumber + 1;
            uint seq_num = ts_packet.GetSequenceNumber();
            if (m_lastSequenceNumber &&
                ((exp_seq_num&0xFFFF) != (seq_num&0xFFFF)))
            {
                LOG(VB_RECORD, LOG_INFO, LOC_WH +
                    QString("Sequence number mismatch %1!=%2")
                    .arg(seq_num).arg(exp_seq_num));
                if (seq_num > exp_seq_num)
                {
                    m_lostInterval = seq_num - exp_seq_num;
                    m_lost += m_lostInterval;
                }
            }
            m_lastSequenceNumber = seq_num;
            m_lastTimestamp = ts_packet.GetTimeStamp();
            LOG(VB_RECORD, LOG_DEBUG,
                QString("Processing RTP packet(seq:%1 ts:%2)")
                .arg(m_lastSequenceNumber).arg(m_lastTimestamp));

            m_parent->m_listenerLock.lock();

            int remainder = 0;
            for (auto sit = m_parent->m_streamDataList.cbegin();
                 sit != m_parent->m_streamDataList.cend(); ++sit)
            {
                remainder = sit.key()->ProcessData(
                    ts_packet.GetTSData(), ts_packet.GetTSDataSize());
            }

            m_parent->m_listenerLock.unlock();

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
    if (m_parent->m_rtcpDest.isNull())
    {
        // no point sending data if we don't know where to
        return;
    }
    int seq_delta = m_lastSequenceNumber - m_previousLastSequenceNumber;
    RTCPDataPacket rtcp =
        RTCPDataPacket(m_lastTimestamp, m_lastTimestamp + RTCP_TIMER.count(),
                       m_lastSequenceNumber, m_lastSequenceNumber + seq_delta,
                       m_lost, m_lostInterval, m_parent->m_rtspSsrc);
    QByteArray buf = rtcp.GetData();

    LOG(VB_RECORD, LOG_DEBUG, LOC_WH +
        QString("Sending RTCPReport to %1:%2")
        .arg(m_parent->m_rtcpDest.toString())
        .arg(m_parent->m_rtspRtcpPort));
    m_parent->m_sockets[1]->writeDatagram(buf.constData(), buf.size(),
                                          m_parent->m_rtcpDest,
                                          m_parent->m_rtspRtcpPort);
    m_previousLastSequenceNumber = m_lastSequenceNumber;
}
