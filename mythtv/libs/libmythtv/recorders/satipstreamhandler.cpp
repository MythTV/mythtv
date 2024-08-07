// -*- Mode: c++ -*-

// C++ headers
#include <chrono>
#include <thread>

// Qt headers
#include <QString>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QUdpSocket>

// MythTV headers
#include "libmythbase/mythlogging.h"

#include "cardutil.h"
#include "dtvsignalmonitor.h"
#include "rtp/rtptsdatapacket.h"
#include "satiputils.h"
#include "satipchannel.h"
#include "satipstreamhandler.h"
#include "satiprtcppacket.h"

#define LOC QString("SatIPSH[%1]: ").arg(m_inputId)

// For implementing Get & Return
QMap<QString, SatIPStreamHandler*> SatIPStreamHandler::s_handlers;
QMap<QString, uint>                SatIPStreamHandler::s_handlersRefCnt;
QMutex                             SatIPStreamHandler::s_handlersLock;

SatIPStreamHandler *SatIPStreamHandler::Get(const QString &devname, int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    QMap<QString, SatIPStreamHandler*>::iterator it = s_handlers.find(devname);

    if (it == s_handlers.end())
    {
        auto *newhandler = new SatIPStreamHandler(devname, inputid);
        newhandler->Open();
        s_handlers[devname] = newhandler;
        s_handlersRefCnt[devname] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("SatIPSH[%1]: Creating new stream handler for %2")
            .arg(inputid).arg(devname));
    }
    else
    {
        s_handlersRefCnt[devname]++;
        uint rcount = s_handlersRefCnt[devname];
        (*it)->m_inputId = inputid;

        LOG(VB_RECORD, LOG_INFO,
            QString("SatIPSH[%1]: Using existing stream handler for %2").arg(inputid).arg(devname) +
            QString(" (%1 users)").arg(rcount));
    }

    return s_handlers[devname];
}

void SatIPStreamHandler::Return(SatIPStreamHandler * & ref, int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    QString devname = ref->m_device;

    QMap<QString, uint>::iterator rit = s_handlersRefCnt.find(devname);
    if (rit == s_handlersRefCnt.end())
    {
        LOG(VB_RECORD, LOG_ERR, QString("SatIPSH[%1]: Return(%2) not found")
            .arg(inputid).arg(devname));
        return;
    }

    LOG(VB_RECORD, LOG_INFO, QString("SatIPSH[%1]: Return stream handler for %2 (%3 users)")
        .arg(inputid).arg(devname).arg(*rit));

    if (*rit > 1)
    {
        ref = nullptr;
        (*rit)--;
        return;
    }

    QMap<QString, SatIPStreamHandler*>::iterator it = s_handlers.find(devname);
    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("SatIPSH[%1]: Closing handler for %2")
            .arg(inputid).arg(devname));
        (*it)->Stop();
        (*it)->Close();
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("SatIPSH[%1] Error: Couldn't find handler for %2")
            .arg(inputid).arg(devname));
    }

    s_handlersRefCnt.erase(rit);
    ref = nullptr;
}

SatIPStreamHandler::SatIPStreamHandler(const QString &device, int inputid)
    : StreamHandler(device, inputid)
    , m_inputId(inputid)
    , m_device(device)
{
    setObjectName("SatIPStreamHandler");

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("ctor for %2").arg(device));

    // Find the port to use for receiving the RTP data.
    // First try a fixed even port number outside the range of dynamically allocated ports.
    // If this fails try to get a dynamically allocated port.
    uint preferred_port = 26420 + (2*inputid);
    m_dsocket = new QUdpSocket(nullptr);
    if (m_dsocket->bind(QHostAddress::AnyIPv4,
                        preferred_port,
                        QAbstractSocket::DefaultForPlatform))
    {
        m_dport = m_dsocket->localPort();
    }
    else
    {
        if (m_dsocket->bind(QHostAddress::AnyIPv4,
                            0,
                            QAbstractSocket::DefaultForPlatform))
        {
            m_dport = m_dsocket->localPort();
        }
    }

    // Messages
    if (m_dport == preferred_port)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("RTP socket bound to requested port %1").arg(m_dport));
    }
    else if (m_dport > 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Requested port %1 but RTP socket bound to port %2")
                .arg(preferred_port).arg(m_dport));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to bind RTP socket"));
        return;
    }

    // ------------------------------------------------------------------------

    // Control socket is next higher port; if we cannot bind do this port
    // then try to bind to a port from the dynamic range
    preferred_port = m_dport + 1;
    m_csocket = new QUdpSocket(nullptr);

    if (m_csocket->bind(QHostAddress::AnyIPv4,
                       preferred_port,
                       QAbstractSocket::DefaultForPlatform))
    {
        m_cport = m_csocket->localPort();
    }
    else
    {
        if (m_csocket->bind(QHostAddress::AnyIPv4,
                            0,
                            QAbstractSocket::DefaultForPlatform))
        {
            m_cport = m_csocket->localPort();
        }
    }

    // Messages
    if (m_cport == preferred_port)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("RTCP socket bound to requested port %1").arg(m_cport));
    }
    else if (m_cport > 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Requested port %1 but RTCP socket bound to port %2")
                .arg(preferred_port).arg(m_cport));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to bind RTCP socket"));
    }

    // If the second port is not one more than the first port we are violating the SatIP standard.
    // Possibly we should then redo the complete port binding.

    // Increase receive packet buffer size for the RTP data stream to prevent packet loss
    // Set UDP socket buffer size big enough to avoid buffer overrun.
    // Buffer size can be reduced if and when the readhelper is running on a separate thread.
    const uint desiredsize = 8*1000*1000;
    const uint newsize = SatIPStreamHandler::SetUDPReceiveBufferSize(m_dsocket, desiredsize);
    if (newsize < desiredsize)
    {
        static bool msgdone = false;

        if (!msgdone)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "RTP UDP socket receive buffer too small\n" +
                QString("\tRTP UDP socket receive buffer size set to %1 but requested %2\n").arg(newsize).arg(desiredsize) +
                QString("\tTo prevent UDP packet loss increase net.core.rmem_max e.g. with this command:\n") +
                QString("\tsudo sysctl -w net.core.rmem_max=%1\n").arg(desiredsize) +
                QString("\tand restart mythbackend."));
            msgdone = true;
        }
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("RTP UDP socket receive buffer size is %1").arg(newsize));
    }

    // Create the read helpers
    m_dataReadHelper = new SatIPDataReadHelper(this);
    m_controlReadHelper = new SatIPControlReadHelper(this);

    // Create the RTSP handler
    m_rtsp = new SatIPRTSP(m_inputId);
}

SatIPStreamHandler::~SatIPStreamHandler()
{
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("dtor for %2").arg(m_device));
    delete m_controlReadHelper;
    delete m_dataReadHelper;
}

bool SatIPStreamHandler::UpdateFilters(void)
{
#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC + "UpdateFilters()");
#endif // DEBUG_PID_FILTERS
    QMutexLocker locker(&m_pidLock);

    QStringList pids;

    if (m_pidInfo.contains(0x2000))
    {
        pids.append("all");
    }
    else
    {
        for (auto it = m_pidInfo.cbegin(); it != m_pidInfo.cend(); ++it)
            pids.append(QString("%1").arg(it.key()));
    }
#ifdef DEBUG_PID_FILTERS
    QString msg = QString("PIDS: '%1'").arg(pids.join(","));
    LOG(VB_RECORD, LOG_DEBUG, LOC + msg);
#endif // DEBUG_PID_FILTERS

    bool rval = true;
    if (m_rtsp && m_oldpids != pids)
    {
        QString pids_str = QString("pids=%1").arg(!pids.empty() ? pids.join(",") : "none");
        LOG(VB_RECORD, LOG_INFO, LOC + "Play(pids_str) " + pids_str);

        // Telestar Digibit R1 Sat>IP box cannot handle a lot of pids
        if (pids.size() > 32)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Receive full TS, number of PIDs:%1 is more than 32").arg(pids.size()));
            LOG(VB_RECORD, LOG_DEBUG, LOC + pids_str);
            pids_str = QString("pids=all");
        }

        rval = m_rtsp->Play(pids_str);
        m_oldpids = pids;
    }

    return rval;
}

void SatIPStreamHandler::run(void)
{
    RunProlog();

    SetRunning(true, false, false);

    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): begin");

    QElapsedTimer last_update;

    while (m_runningDesired && !m_bError)
    {
        {
            QMutexLocker locker(&m_tunelock);

            if (m_oldtuningurl != m_tuningurl)
            {
                if (m_setupinvoked)
                {
                    m_rtsp->Teardown();
                    m_setupinvoked = false;
                }

                if (m_rtsp->Setup(m_tuningurl, m_dport, m_cport))
                {
                    m_oldtuningurl = m_tuningurl;
                    m_setupinvoked = true;
                }

                last_update.restart();
            }
        }

        // Update the PID filters every 100 milliseconds
        auto elapsed = !last_update.isValid()
            ? -1ms : std::chrono::milliseconds(last_update.elapsed());
        elapsed = (elapsed < 0ms) ? 1s : elapsed;
        if (elapsed > 100ms)
        {
            UpdateFiltersFromStreamData();
            UpdateFilters();
            last_update.restart();
        }

        // Delay to avoid busy wait loop
        std::this_thread::sleep_for(20ms);

    }
    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): " + "shutdown");

    RemoveAllPIDFilters();

    // TEARDOWN command
    if (m_setupinvoked)
    {
        QMutexLocker locker(&m_tunelock);
        m_rtsp->Teardown();
        m_setupinvoked = false;
        m_oldtuningurl = "";
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): end");
    SetRunning(false, false, false);
    RunEpilog();
}

bool SatIPStreamHandler::Tune(const DTVMultiplex &tuning)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Tune %1").arg(tuning.m_frequency));

    QMutexLocker locker(&m_tunelock);

    // Build the query string
    QStringList qry;

    if (m_tunerType == DTVTunerType::kTunerTypeDVBC)
    {
        qry.append(QString("fe=%1").arg(m_frontend+1));
        qry.append(QString("freq=%1").arg(SatIP::freq(tuning.m_frequency)));
        qry.append(QString("sr=%1").arg(tuning.m_symbolRate / 1000)); // symbolrate in ksymb/s
        qry.append("msys=dvbc");
        qry.append(QString("mtype=%1").arg(SatIP::mtype(tuning.m_modulation)));
    }
    else if (m_tunerType == DTVTunerType::kTunerTypeDVBT || m_tunerType == DTVTunerType::kTunerTypeDVBT2)
    {
        qry.append(QString("fe=%1").arg(m_frontend+1));
        qry.append(QString("freq=%1").arg(SatIP::freq(tuning.m_frequency)));
        qry.append(QString("bw=%1").arg(SatIP::bw(tuning.m_bandwidth)));
        qry.append(QString("msys=%1").arg(SatIP::msys(tuning.m_modSys)));
        qry.append(QString("tmode=%1").arg(SatIP::tmode(tuning.m_transMode)));
        qry.append(QString("mtype=%1").arg(SatIP::mtype(tuning.m_modulation)));
        qry.append(QString("gi=%1").arg(SatIP::gi(tuning.m_guardInterval)));
        qry.append(QString("fec=%1").arg(SatIP::fec(tuning.m_fec)));
    }
    else if (m_tunerType == DTVTunerType::kTunerTypeDVBS1 || m_tunerType == DTVTunerType::kTunerTypeDVBS2)
    {
        qry.append(QString("fe=%1").arg(m_frontend+1));
        qry.append(QString("src=%1").arg(m_satipsrc));
        qry.append(QString("freq=%1").arg(SatIP::freq(tuning.m_frequency*1000)));   // frequency in Hz
        qry.append(QString("pol=%1").arg(SatIP::pol(tuning.m_polarity)));
        qry.append(QString("ro=%1").arg(SatIP::ro(tuning.m_rolloff)));
        qry.append(QString("msys=%1").arg(SatIP::msys(tuning.m_modSys)));
        qry.append(QString("mtype=%1").arg(SatIP::mtype(tuning.m_modulation)));
        qry.append(QString("sr=%1").arg(tuning.m_symbolRate / 1000));               // symbolrate in ksymb/s
        qry.append(QString("fec=%1").arg(SatIP::fec(tuning.m_fec)));
        qry.append(QString("plts=auto"));                                           // pilot tones
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("Unhandled m_tunerType %1 %2").arg(m_tunerType).arg(m_tunerType.toString()));
        return false;
    }

    QUrl url = QUrl(m_baseurl);
    url.setQuery(qry.join("&"));

    m_tuningurl = url;

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Tune url:%1").arg(url.toString()));

    if (m_tuningurl == m_oldtuningurl)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Skip tuning, already tuned to this url"));
        return true;
    }

    // Need SETUP and PLAY (with pids=none) to get RTSP packets with tuner lock info
    if (m_rtsp)
    {
        bool rval = true;

        // TEARDOWN command
        if (m_setupinvoked)
        {
            rval = m_rtsp->Teardown();
            m_setupinvoked = false;
        }

        // SETUP command
        if (rval)
        {
            rval = m_rtsp->Setup(m_tuningurl, m_dport, m_cport);
        }
        if (rval)
        {
            m_oldtuningurl = m_tuningurl;
            m_setupinvoked = true;
        }

        // PLAY command
        if (rval)
        {
            m_rtsp->Play("pids=none");
            m_oldpids = QStringList();
        }
        return rval;
    }
    return true;
}

bool SatIPStreamHandler::Open(void)
{
    QUrl url;
    url.setScheme("rtsp");
    url.setPort(554);
    url.setPath("/");

    // Discover the device using SSDP
    QStringList devinfo = m_device.split(":");
    if (devinfo.value(0).toUpper() == "UUID")
    {
        QString deviceId = QString("uuid:%1").arg(devinfo.value(1));
        m_frontend = devinfo.value(3).toUInt();

        QString ip = SatIP::findDeviceIP(deviceId);
        if (ip != nullptr)
        {
            LOG(VB_RECORD, LOG_INFO, LOC + QString("Discovered device %1 at %2").arg(deviceId, ip));
        }
        else
        {
            LOG(VB_RECORD, LOG_ERR, LOC + QString("Failed to discover device %1, no IP found").arg(deviceId));
            return false;
        }

        url.setHost(ip);
    }
    else
    {
        // TODO: Handling of manual IP devices
    }

    m_tunerType = SatIP::toTunerType(m_device);
    m_baseurl = url;

    return true;
}

void SatIPStreamHandler::Close(void)
{
    delete m_rtsp;
    m_rtsp = nullptr;
    m_baseurl = nullptr;
}

bool SatIPStreamHandler::HasLock()
{
    QMutexLocker locker(&m_sigmonLock);
    return m_hasLock;
}

int SatIPStreamHandler::GetSignalStrength()
{
    QMutexLocker locker(&m_sigmonLock);
    return m_signalStrength;
}

void SatIPStreamHandler::SetSigmonValues(bool lock, int level)
{
    QMutexLocker locker(&m_sigmonLock);
    m_hasLock = lock;
    m_signalStrength = level;
}

// === RTP DataReadHelper ===================================================
//
// Read RTP stream data from the UDP socket and store it in the packet buffer
// and write the packets immediately to the listeners.
//
// TODO
// This has to be created in a separate thread to achieve minimum
// minimum latency and so to avoid overflow of the UDP input buffers.
// N.B. Then also with the socket that is now in the streamhandler.
// ---------------------------------------------------------------------------

#define LOC_DRH QString("SH_DRH[%1]: ").arg(m_streamHandler->m_inputId)

SatIPDataReadHelper::SatIPDataReadHelper(SatIPStreamHandler* handler)
    : m_streamHandler(handler)
    , m_socket(handler->m_dsocket)
{
    LOG(VB_RECORD, LOG_INFO, LOC_DRH +
        QString("Starting data read helper for RTP UDP socket"));

    // Call ReadPending when there are RTP data packets received on m_socket
    connect(m_socket, &QIODevice::readyRead,
            this,     &SatIPDataReadHelper::ReadPending);

    // Number of RTP packets to discard at start.
    // This is to flush the RTP packets that might still be in transit
    // from the previously tuned channel.
    m_count = 3;
    m_valid = false;

    LOG(VB_RECORD, LOG_DEBUG, LOC_DRH + QString("Init flush count to %1").arg(m_count));
}

SatIPDataReadHelper::~SatIPDataReadHelper()
{
    LOG(VB_RECORD, LOG_INFO, LOC_DRH + QString("%1").arg(__func__));
    disconnect(m_socket, &QIODevice::readyRead,
               this,     &SatIPDataReadHelper::ReadPending);
}

void SatIPDataReadHelper::ReadPending()
{
#if 0
    LOG(VB_RECORD, LOG_INFO, LOC_RH + QString("%1").arg(__func__));
#endif

    RTPDataPacket pkt;

    while (m_socket->hasPendingDatagrams())
    {
#if 0
        LOG(VB_RECORD, LOG_INFO, LOC_DRH + QString("%1 hasPendingDatagrams").arg(__func__));
#endif
        QHostAddress sender;
        quint16 senderPort = 0;

        QByteArray &data = pkt.GetDataReference();
        data.resize(m_socket->pendingDatagramSize());
        m_socket->readDatagram(data.data(), data.size(), &sender, &senderPort);

        if (pkt.GetPayloadType() == RTPDataPacket::kPayLoadTypeTS)
        {
            RTPTSDataPacket ts_packet(pkt);

            if (!ts_packet.IsValid())
            {
                continue;
            }

            // Check the packet sequence number
            uint expectedSequenceNumber = (m_sequenceNumber + 1) & 0xFFFF;
            m_sequenceNumber = ts_packet.GetSequenceNumber();
            if ((expectedSequenceNumber != m_sequenceNumber) && m_valid)
            {
                LOG(VB_RECORD, LOG_ERR, LOC_DRH +
                    QString("Sequence number error -- Expected:%1 Received:%2")
                        .arg(expectedSequenceNumber).arg(m_sequenceNumber));
            }

            // Flush the first few packets after start
            if (m_count > 0)
            {
                LOG(VB_RECORD, LOG_INFO, LOC_DRH + QString("Flushing RTP packet, %1 to do").arg(m_count));
                m_count--;
            }
            else
            {
                m_valid = true;
            }

            // Send the packet data to all listeners
            if (m_valid)
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
                    LOG(VB_RECORD, LOG_INFO, LOC_DRH +
                        QString("RTP data_length = %1 remainder = %2")
                        .arg(ts_packet.GetTSDataSize()).arg(remainder));
                }
            }
        }
    }
}


// === RTSP RTCP ControlReadHelper ===========================================
//
// Read RTCP packets with control messages from the UDP socket.
// Determine tuner state: lock and signal strength
// ---------------------------------------------------------------------------

#define LOC_CRH QString("SatIP_CRH[%1]: ").arg(m_streamHandler->m_inputId)

SatIPControlReadHelper::SatIPControlReadHelper(SatIPStreamHandler *handler)
    : m_streamHandler(handler)
    , m_socket(handler->m_csocket)
{
    LOG(VB_RECORD, LOG_INFO, LOC_CRH +
        QString("Starting read helper for RTCP UDP socket"));

    // Call ReadPending when there is a message received on m_socket
    connect(m_socket, &QUdpSocket::readyRead,
            this,     &SatIPControlReadHelper::ReadPending);
}

SatIPControlReadHelper::~SatIPControlReadHelper()
{
    LOG(VB_RECORD, LOG_INFO, LOC_CRH + QString("%1").arg(__func__));
    disconnect(m_socket, &QIODevice::readyRead,
               this,     &SatIPControlReadHelper::ReadPending);
}

// Process a RTCP packet received on m_socket
void SatIPControlReadHelper::ReadPending()
{
    while (m_socket->hasPendingDatagrams())
    {
#if 0
        LOG(VB_RECORD, LOG_INFO, LOC_CRH +
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
            LOG(VB_GENERAL, LOG_ERR, LOC_CRH + "Invalid RTCP packet received");
            continue;
        }

        QStringList data = pkt.Data().split(";");
        bool found = false;
        int i = 0;

#if 0
        LOG(VB_RECORD, LOG_DEBUG, LOC_CRH + QString(">2 %1 ").arg(__func__) + data.join('^'));
#endif
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

                    LOG(VB_RECORD, LOG_DEBUG, LOC_CRH +
                        QString("Tuner lock:%1 level:%2 quality:%3").arg(lock).arg(level).arg(quality));

                    m_streamHandler->SetSigmonValues(lock, level);
                }
            }
            i++;
        }
    }
}

// ===========================================================================

/** \fn SatIPStreamHandler::GetUDPReceiveBufferSize(uint inputid)
 *  \brief Get receive buffer size of UDP socket
 *  \param socket Socket
 *  \return Actual buffer size
 *
 * Note that the size returned by ReceiverBufferSizeSocketOption is
 * twice the actual buffer size hence the divide by two.
 */
uint SatIPStreamHandler::GetUDPReceiveBufferSize(QUdpSocket *socket)
{
    QVariant ss = socket->socketOption(QAbstractSocket::ReceiveBufferSizeSocketOption);
    return ss.toUInt()/2;
}

/** \fn SatIPStreamHandler::SetUDPReceiveBufferSize(uint inputid)
 *  \brief Set receive buffer size of UDP socket
 *  \param socket Socket
 *  \param rcvbuffersize  Requested buffer size in bytes
 *  \return Actual buffer size
 *
 * Set the UDP buffer to the requested buffer size if the requested
 * buffer size is larger than the current buffer size.
 */
uint SatIPStreamHandler::SetUDPReceiveBufferSize(QUdpSocket *socket, uint rcvbuffersize)
{
    uint oldsize = SatIPStreamHandler::GetUDPReceiveBufferSize(socket);
    if (rcvbuffersize > oldsize)
    {
        socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, rcvbuffersize);
    }
    return SatIPStreamHandler::GetUDPReceiveBufferSize(socket);
}
