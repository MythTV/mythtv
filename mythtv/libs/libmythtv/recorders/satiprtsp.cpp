/** -*- Mode: c++ -*- */

// C++ includes
#include <chrono>

// Qt includes
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QUrl>
#include <QVector>

// MythTV includes
#include "satiprtsp.h"
#include "mythlogging.h"
#include "mythsocket.h"
#include "rtppacketbuffer.h"
#include "udppacketbuffer.h"
#include "rtptsdatapacket.h"
#include "satipstreamhandler.h"
#include "rtcpdatapacket.h"
#include "satiprtcppacket.h"

#if QT_VERSION < QT_VERSION_CHECK(5,15,2)
#define capturedView capturedRef
#endif

#define LOC  QString("SatIPRTSP[%1]: ").arg(m_streamHandler->m_inputId)
#define LOC2 QString("SatIPRTSP[%1](%2): ").arg(m_streamHandler->m_inputId).arg(m_requestUrl.toString())


// Local functions
static uint SetUDPReceiveBufferSize(QUdpSocket *socket, uint rcvbuffersize);


SatIPRTSP::SatIPRTSP(SatIPStreamHandler *handler)
    : m_streamHandler(handler)
{
    uint port = 0;

    connect(this, &SatIPRTSP::startKeepAlive, this, &SatIPRTSP::startKeepAliveRequested);
    connect(this, &SatIPRTSP::stopKeepAlive, this, &SatIPRTSP::stopKeepAliveRequested);

    // Use RTPPacketBuffer if buffering and re-ordering needed
    m_buffer = new UDPPacketBuffer(0);

    m_readHelper = new SatIPRTSPReadHelper(this);
    m_writeHelper = new SatIPRTSPWriteHelper(this, handler);

    if (!m_readHelper->m_socket->bind(QHostAddress::AnyIPv4, 0,
                                      QAbstractSocket::DefaultForPlatform))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to bind RTP socket"));
    }
    else
    {
        port = m_readHelper->m_socket->localPort();
        LOG(VB_RECORD, LOG_INFO, LOC + QString("RTP socket bound to port %1 (0x%2)")
            .arg(port).arg(port,2,16,QChar('0')));
    }

    // Control socket is next higher port
    port++;

    m_rtcpReadHelper = new SatIPRTCPReadHelper(this);
    if (!m_rtcpReadHelper->m_socket->bind(QHostAddress::AnyIPv4,
                                           port,
                                           QAbstractSocket::DefaultForPlatform))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to bind RTCP socket to port %1").arg(port));
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("RTCP socket bound to port %1 (0x%2)")
            .arg(port).arg(port,2,16,QChar('0')));
    }

    // Increase receive packet buffer size for the RTP data stream to prevent packet loss
    uint desiredsize = 8000000;
    uint newsize = SetUDPReceiveBufferSize(m_readHelper->m_socket, desiredsize);
    if (newsize >= desiredsize)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("RTP UDP socket receive buffer size set to %1").arg(newsize));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "RTP UDP socket receive buffer too small\n" +
            QString("\tRTP UDP socket receive buffer size set to %1 but requested %2\n").arg(newsize).arg(desiredsize) +
            QString("\tTo prevent UDP packet loss increase net.core.rmem_max e.g. with this command:\n") +
            QString("\tsudo sysctl -w net.core.rmem_max=%1\n").arg(desiredsize) +
            QString("\tand restart mythbackend."));
    }
}

SatIPRTSP::~SatIPRTSP()
{
    delete m_rtcpReadHelper;
    delete m_writeHelper;
    delete m_readHelper;
    delete m_buffer;
}

bool SatIPRTSP::Setup(const QUrl& url)
{
    m_requestUrl = url;
    LOG(VB_RECORD, LOG_DEBUG, LOC2 + QString("SETUP"));

    if (url.port() != 554)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC + "SatIP specifies RTSP port 554 to be used");
    }

    if (url.port() < 1)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("Invalid port %1").arg(url.port()));
        return false;
    }

    QStringList headers;
    headers.append(
        QString("Transport: RTP/AVP;unicast;client_port=%1-%2")
        .arg(m_readHelper->m_socket->localPort()).arg(m_readHelper->m_socket->localPort() + 1));

    if (!sendMessage(m_requestUrl, "SETUP", &headers))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Failed to send SETUP message");
        return false;
    }

    if (m_headers.contains("COM.SES.STREAMID"))
    {
        m_streamid = m_headers["COM.SES.STREAMID"];
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SETUP response did not contain the com.ses.streamID field");
        return false;
    }

    if (m_headers.contains("SESSION"))
    {
        static const QRegularExpression sessionTimeoutRegex {
            "^([^\\r\\n]+);timeout=([0-9]+)?", QRegularExpression::CaseInsensitiveOption };
        auto match = sessionTimeoutRegex.match(m_headers["SESSION"]);
        if (!match.hasMatch())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Failed to extract session id from session header ('%1')")
                    .arg(m_headers["Session"]));
        }

        m_sessionid = match.captured(1);
        m_timeout = match.capturedLength(2) > 0
            ? std::chrono::seconds(match.capturedView(2).toInt() / 2)
            : 30s;

        LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Sat>IP protocol timeout:%1 ms")
            .arg(m_timeout.count()));
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SETUP response did not contain the Session field");
        return false;
    }

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Setup completed, sessionID = %1, streamID = %2, timeout = %3s")
            .arg(m_sessionid, m_streamid)
            .arg(duration_cast<std::chrono::seconds>(m_timeout).count()));
    emit startKeepAlive(m_timeout);

    // Reset tuner lock status
    QMutexLocker locker(&m_sigmonLock);
    m_hasLock = false;

    return true;
}

bool SatIPRTSP::Play(QStringList &pids)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC2 + "PLAY");

    QUrl url = QUrl(m_requestUrl);
    url.setQuery("");
    url.setPath(QString("/stream=%1").arg(m_streamid));

    QString pids_str = QString("pids=%1").arg(!pids.empty() ? pids.join(",") : "none");
    LOG(VB_RECORD, LOG_INFO, LOC + "Play(pids) " + pids_str);

    // Telestar Digibit R1 Sat>IP box cannot handle a lot of pids
    if (pids.size() > 32)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Receive full TS, number of PIDs:%1 is more than 32").arg(pids.size()));
        LOG(VB_RECORD, LOG_DEBUG, LOC + pids_str);
        pids_str = QString("pids=all");
    }
    url.setQuery(pids_str);
    LOG(VB_RECORD, LOG_DEBUG, LOC + pids_str);

    if (!sendMessage(url, "PLAY"))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Failed to send PLAY message");
        return false;
    }

    // Start processing packets when pids are specified
    m_valid = !pids.empty();

    return true;
}

bool SatIPRTSP::Teardown(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC2 + "TEARDOWN");
    emit stopKeepAlive();

    QUrl url = QUrl(m_requestUrl);
    url.setQuery(QString());
    url.setPath(QString("/stream=%1").arg(m_streamid));

    bool result = sendMessage(url, "TEARDOWN");

    if (!result)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Teardown failed");
    }

    if (result)
    {
        m_sessionid = "";
        m_streamid = "";
    }

    // Discard all RTP packets until the next lock
    m_valid = false;
    m_validOld = false;

    // Reset tuner lock status
    QMutexLocker locker(&m_sigmonLock);
    m_hasLock = false;

    return result;
}

bool SatIPRTSP::HasLock()
{
    QMutexLocker locker(&m_sigmonLock);
    return m_hasLock;
}

int SatIPRTSP::GetSignalStrength()
{
    QMutexLocker locker(&m_sigmonLock);
    return m_signalStrength;
}

void SatIPRTSP::SetSigmonValues(bool lock, int level)
{
    QMutexLocker locker(&m_sigmonLock);
    m_hasLock = lock;
    m_signalStrength = level;
}

bool SatIPRTSP::sendMessage(const QUrl& url, const QString& msg, QStringList *additionalheaders)
{
    QMutexLocker locker(&m_ctrlSocketLock);

    QTcpSocket ctrl_socket;
    ctrl_socket.connectToHost(url.host(), url.port());

    bool ok = ctrl_socket.waitForConnected(30 * 1000);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not connect to server %1:%2").arg(url.host()).arg(url.port()));
        return false;
    }

    QStringList headers;
    headers.append(QString("%1 %2 RTSP/1.0").arg(msg, url.toString()));
    headers.append(QString("User-Agent: MythTV Sat>IP client"));
    headers.append(QString("CSeq: %1").arg(++m_cseq));

    if (m_sessionid.length() > 0)
    {
        headers.append(QString("Session: %1").arg(m_sessionid));
    }

    if (additionalheaders != nullptr)
    {
        for (auto& adhdr : *additionalheaders)
        {
            headers.append(adhdr);
        }
    }

    headers.append("\r\n");

    for (const auto& requestLine : headers)
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + "sendMessage " +
            QString("write: %1").arg(requestLine.simplified()));
    }

    QString request = headers.join("\r\n");
    ctrl_socket.write(request.toLatin1());

    QRegularExpression firstLineRE { "^RTSP/1.0 (\\d+) ([^\r\n]+)" };
    QRegularExpression headerRE    { R"(^([^:]+):\s*([^\r\n]+))"   };
    QRegularExpression blankLineRE { R"(^[\r\n]*$)"                };

    bool firstLine = true;
    while (true)
    {
        if (!ctrl_socket.canReadLine())
        {
            bool ready = ctrl_socket.waitForReadyRead(10 * 1000);
            if (!ready)
            {
                LOG(VB_RECORD, LOG_ERR, LOC + "RTSP server did not respond after 10s");
                return false;
            }
            continue;
        }

        QString line = ctrl_socket.readLine();
        LOG(VB_RECORD, LOG_DEBUG, LOC + "sendMessage " +
            QString("read: %1").arg(line.simplified()));

        QRegularExpressionMatch match;
        if (firstLine)
        {
            match = firstLineRE.match(line);
            if (!match.hasMatch())
            {
                return false;
            }

            QStringList parts = match.capturedTexts();
            int responseCode = parts.at(1).toInt();
            const QString& responseMsg = parts.at(2);

            LOG(VB_RECORD, LOG_DEBUG, LOC + QString("response code:%1 message:%2")
                .arg(responseCode).arg(responseMsg));

            if (responseCode != 200)
            {
                return false;
            }
            firstLine = false;
            continue;
        }

        match = blankLineRE.match(line);
        if (match.hasMatch()) break;

        match = headerRE.match(line);
        if (!match.hasMatch())
        {
            return false;
        }
        QStringList parts = match.capturedTexts();
        m_headers.insert(parts.at(1).toUpper(), parts.at(2));
    }

    QString cSeq;

    if (m_headers.contains("CSEQ"))
    {
        cSeq = m_headers["CSEQ"];
    }

    if (cSeq != QString("%1").arg(m_cseq))
    {
        LOG(VB_RECORD, LOG_WARNING, LOC + QString("Expected CSeq of %1 but got %2").arg(m_cseq).arg(cSeq));
    }

    ctrl_socket.disconnectFromHost();
    if (ctrl_socket.state() != QAbstractSocket::UnconnectedState)
    {
        ctrl_socket.waitForDisconnected();
    }

    return true;
}

void SatIPRTSP::startKeepAliveRequested(std::chrono::milliseconds timeout)
{
    if (m_timer)
        return;
    m_timer = startTimer(timeout);
    LOG(VB_RECORD, LOG_INFO, LOC + QString("startKeepAliveRequested(%1) m_timer:%2")
        .arg(timeout.count()).arg(m_timer));
}

void SatIPRTSP::stopKeepAliveRequested()
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("stopKeepAliveRequested() m_timer:%1").arg(m_timer));
    if (m_timer)
    {
        killTimer(m_timer);
    }
    m_timer = 0;
}

void SatIPRTSP::timerEvent(QTimerEvent* timerEvent)
{
    (void) timerEvent;
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Sending KeepAlive timer %1").arg(timerEvent->timerId()));

    QUrl url = QUrl(m_requestUrl);
    url.setPath("/");
    url.setQuery("");

    sendMessage(url, "OPTIONS");
}

// --- RTSP RTP ReadHelper ---------------------------------------------------
//
// Receive RTP packets with stream data on UDP socket.
// Read packets when signalled via readyRead on QUdpSocket.
//
#define LOC_RH QString("SatIPRTSP[%1]: ").arg(m_parent->m_streamHandler->m_inputId)

SatIPRTSPReadHelper::SatIPRTSPReadHelper(SatIPRTSP* p)
    : QObject(p)
    , m_socket(new QUdpSocket(this))
    , m_parent(p)
{
    LOG(VB_RECORD, LOG_INFO, LOC_RH +
        QString("Starting read helper for UDP (RTP) socket"));

    // Call ReadPending when there is a message received on m_socket
    connect(m_socket, &QUdpSocket::readyRead, this, &SatIPRTSPReadHelper::ReadPending);
}

SatIPRTSPReadHelper::~SatIPRTSPReadHelper()
{
    delete m_socket;
}

void SatIPRTSPReadHelper::ReadPending()
{
    while (m_socket->hasPendingDatagrams())
    {
        QHostAddress sender;
        quint16 senderPort = 0;

        UDPPacket packet(m_parent->m_buffer->GetEmptyPacket());
        QByteArray &data = packet.GetDataReference();
        data.resize(m_socket->pendingDatagramSize());
        m_socket->readDatagram(data.data(), data.size(), &sender, &senderPort);
        m_parent->m_buffer->PushDataPacket(packet);
    }
}

// --- RTSP RTCP ReadHelper --------------------------------------------------
//
// Receive RTCP packets with control messages on UDP socket
// Receive tuner state: lock and signal strength
//
#define LOC_RTCP QString("SatIPRTCP[%1]: ").arg(m_parent->m_streamHandler->m_inputId)

SatIPRTCPReadHelper::SatIPRTCPReadHelper(SatIPRTSP* p)
    : QObject(p)
    , m_socket(new QUdpSocket(this))
    , m_parent(p)
{
    LOG(VB_RECORD, LOG_INFO, LOC_RTCP +
        QString("Starting read helper for UDP (RTCP) socket"));

    // Call ReadPending when there is a message received on m_socket
    connect(m_socket, &QUdpSocket::readyRead, this, &SatIPRTCPReadHelper::ReadPending);
}

SatIPRTCPReadHelper::~SatIPRTCPReadHelper()
{
    delete m_socket;
}

// Process a RTCP packet received on m_socket
void SatIPRTCPReadHelper::ReadPending()
{
    while (m_socket->hasPendingDatagrams())
    {
#if 0
        LOG(VB_RECORD, LOG_INFO, "SatIPRTSP_RH " +
            QString("Processing RTCP packet(pendingDatagramSize:%1)")
            .arg(m_socket->pendingDatagramSize()));
#endif

        QHostAddress sender;
        quint16 senderPort = 0;

        QByteArray buf = QByteArray(m_socket->pendingDatagramSize(), Qt::Uninitialized);
        m_socket->readDatagram(buf.data(), buf.size(), &sender, &senderPort);

        SatIPRTCPPacket pkt = SatIPRTCPPacket(buf);
        if (!pkt.IsValid())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC_RTCP + "Invalid RTCP packet received");
            continue;
        }

        QStringList data = pkt.Data().split(";");
        bool found = false;
        int i = 0;

        while (!found && i < data.length())
        {
            const QString& item = data.at(i);

            if (item.startsWith("tuner="))
            {
                found = true;
                QStringList tuner = item.split(",");

                if (tuner.length() > 3)
                {
                    int level = tuner.at(1).toInt();        // [0, 255]
                    bool lock = tuner.at(2).toInt() != 0;   // [0 , 1]
                    int quality = tuner.at(3).toInt();      // [0, 15]

                    LOG(VB_RECORD, LOG_DEBUG, LOC_RTCP +
                        QString("Tuner lock:%1 level:%2 quality:%3").arg(lock).arg(level).arg(quality));

                    m_parent->SetSigmonValues(lock, level);
                }
            }
            i++;
        }
    }
}

// --- SatIPRTSPWriteHelper --------------------------------------------------
//
#define LOC_WH QString("SatIPRTSP[%1]: ").arg(m_streamHandler->m_inputId)

SatIPRTSPWriteHelper::SatIPRTSPWriteHelper(SatIPRTSP* parent, SatIPStreamHandler* handler)
    : m_parent(parent)
    , m_streamHandler(handler)
{
    m_timer = startTimer(100ms);
}

void SatIPRTSPWriteHelper::timerEvent(QTimerEvent* /*event*/)
{
    while (m_parent->m_buffer->HasAvailablePacket())
    {
        RTPDataPacket pkt(m_parent->m_buffer->PopDataPacket());

        if (pkt.GetPayloadType() == RTPDataPacket::kPayLoadTypeTS)
        {
            RTPTSDataPacket ts_packet(pkt);

            if (!ts_packet.IsValid())
            {
                m_parent->m_buffer->FreePacket(pkt);
                continue;
            }

            // Check for sequence number
            uint exp_seq_num = m_lastSequenceNumber + 1;
            uint seq_num = ts_packet.GetSequenceNumber();
            if (m_lastSequenceNumber &&
                ((exp_seq_num & 0xFFFF) != (seq_num & 0xFFFF)))
            {
                // Discard all pending packets to recover from buffer overflow
                m_parent->m_buffer->FreePacket(pkt);
                uint discarded = 1;
                while (m_parent->m_buffer->HasAvailablePacket())
                {
                    RTPDataPacket pkt_next(m_parent->m_buffer->PopDataPacket());
                    m_parent->m_buffer->FreePacket(pkt);
                    discarded++;
                }
                m_lastSequenceNumber = 0;

                LOG(VB_RECORD, LOG_INFO, LOC_WH +
                    QString("RTP Sequence number mismatch %1!=%2, discarded %3 RTP packets")
                    .arg(seq_num).arg(exp_seq_num).arg(discarded));
                continue;
            }
            m_lastSequenceNumber = ts_packet.GetSequenceNumber();
#if 0
            m_lastTimeStamp = ts_packet.GetTimeStamp();
            LOG(VB_RECORD, LOG_INFO, LOC_WH +
                QString("Processing RTP packet(seq:%1 ts:%2, ts_data_size:%3) %4")
                .arg(m_lastSequenceNumber).arg(m_lastTimeStamp).arg(ts_packet.GetTSDataSize())
                .arg((m_parent->m_valid && m_parent->m_validOld)?"processed":"discarded"));
#endif
            if (m_parent->m_valid && m_parent->m_validOld)
            {
                int remainder = 0;
                {
                    QMutexLocker locker(&m_streamHandler->m_listenerLock);
                    auto streamDataList = m_streamHandler->m_streamDataList;
                    if (!streamDataList.isEmpty())
                    {
                        const unsigned char *data_buffer = ts_packet.GetTSData();
                        size_t data_length = ts_packet.GetTSDataSize();

                        for (auto sit = streamDataList.cbegin(); sit != streamDataList.cend(); ++sit)
                        {
                            remainder = sit.key()->ProcessData(data_buffer, data_length);
                        }

                        m_streamHandler->WriteMPTS(data_buffer, data_length - remainder);
                    }
                }

                if (remainder != 0)
                {
                    LOG(VB_RECORD, LOG_INFO, LOC_WH +
                        QString("RTP data_length = %1 remainder = %2")
                        .arg(ts_packet.GetTSDataSize()).arg(remainder));
                }
            }
        }
        m_parent->m_buffer->FreePacket(pkt);
    }
    m_parent->m_validOld = m_parent->m_valid;
}

// Set receive buffer size of UDP socket
//
// Note that the size returned by ReceiverBufferSizeSocketOption is
// twice the actual buffer size.
//
static uint SetUDPReceiveBufferSize(QUdpSocket *socket, uint rcvbuffersize)
{
    QVariant ss = socket->socketOption(QAbstractSocket::ReceiveBufferSizeSocketOption);
    uint oldsize = ss.toUInt();
    if (oldsize < rcvbuffersize*2)
    {
        socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, rcvbuffersize);
    }
    ss = socket->socketOption(QAbstractSocket::ReceiveBufferSizeSocketOption);
    uint newsize = ss.toUInt()/2;
    return newsize;
}
