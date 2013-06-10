// -*- Mode: c++ -*-

// System headers
#ifdef _WIN32
# ifndef _MSC_VER
#  include <ws2tcpip.h>
# endif
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
#endif

// Qt headers
#include <QUdpSocket>

// MythTV headers
#include "iptvstreamhandler.h"
#include "rtppacketbuffer.h"
#include "udppacketbuffer.h"
#include "rtptsdatapacket.h"
#include "rtpdatapacket.h"
#include "rtpfecpacket.h"
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
    m_use_rtp_streaming(true)
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
        if (!(rtsp->GetOptions(options)     && options.contains("OPTIONS")  &&
              options.contains("DESCRIBE")  && options.contains("SETUP")    &&
              options.contains("PLAY")      && options.contains("TEARDOWN")))
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

        tuning = IPTVTuningData(
            QString("rtp://%1@%2:0")
            .arg(m_tuning.GetURL(0).host())
            .arg(QHostAddress(QHostAddress::Any).toString()), 0,
            IPTVTuningData::kNone,
            QString("rtp://%1@%2:0")
            .arg(m_tuning.GetURL(0).host())
            .arg(QHostAddress(QHostAddress::Any).toString()), 0,
            "", 0);
    }

    for (uint i = 0; i < IPTV_SOCKET_COUNT; i++)
    {
        QUrl url = tuning.GetURL(i);
        if (url.port() < 0)
            continue;

        m_sockets[i] = new QUdpSocket();
        m_read_helpers[i] = new IPTVStreamHandlerReadHelper(
            this, m_sockets[i], i);

        // we need to open the descriptor ourselves so we
        // can set some socket options
        int fd = socket(AF_INET, SOCK_DGRAM, 0); // create IPv4 socket
        if (fd < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unable to create socket " + ENO);
            continue;
        }
        int buf_size = 2 * 1024 * max(tuning.GetBitrate(i)/1000, 500U);
        if (!tuning.GetBitrate(i))
            buf_size = 2 * 1024 * 1024;
        int ok = setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                            (char *)&buf_size, sizeof(buf_size));
        if (ok)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Increasing buffer size to %1 failed")
                .arg(buf_size) + ENO);
        }
        /*
          int broadcast = 1;
          ok = setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
          (char *)&broadcast, sizeof(broadcast));
          if (ok)
          {
          LOG(VB_GENERAL, LOG_INFO, LOC +
          QString("Enabling broadcast failed") + ENO);
          }
        */
        m_sockets[i]->setSocketDescriptor(
            fd, QAbstractSocket::UnconnectedState, QIODevice::ReadOnly);

        m_sockets[i]->bind(QHostAddress::Any, url.port());

        QHostAddress dest_addr(tuning.GetURL(i).host());

        if (dest_addr != QHostAddress::Any)
        {
            //m_sockets[i]->joinMulticastGroup(dest_addr); // needs Qt 4.8
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Joining %1")
                .arg(dest_addr.toString()));
            struct ip_mreq imr;
            memset(&imr, 0, sizeof(struct ip_mreq));
            imr.imr_multiaddr.s_addr = inet_addr(
                dest_addr.toString().toLatin1().constData());
            imr.imr_interface.s_addr = htonl(INADDR_ANY);
            if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                           &imr, sizeof(imr)) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "setsockopt - IP_ADD_MEMBERSHIP " + ENO);
            }
        }

        if (!url.userInfo().isEmpty())
            m_sender[i] = QHostAddress(url.userInfo());
    }
    if (m_use_rtp_streaming)
        m_buffer = new RTPPacketBuffer(tuning.GetBitrate(0));
    else
        m_buffer = new UDPPacketBuffer(tuning.GetBitrate(0));
    m_write_helper = new IPTVStreamHandlerWriteHelper(this);
    m_write_helper->Start();

    bool error = false;
    if (rtsp)
    {
        // Start Streaming
        if (!rtsp->Setup(m_sockets[0]->localPort(),
                         m_sockets[1]->localPort()) ||
            !rtsp->Play())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "Starting recording (RTP initialization failed). Aborting.");
            error = true;
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
                m_parent->m_buffer->PushDataPacket(packet);
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
                m_parent->m_buffer->PushFECPacket(packet, m_stream - 1);
        }
    }
}

#define LOC_WH QString("IPTVSH(%1): ").arg(m_parent->_device)

void IPTVStreamHandlerWriteHelper::timerEvent(QTimerEvent*)
{
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
            }
            m_last_sequence_number = seq_num;

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
