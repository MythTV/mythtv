#include <QTimer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QtEndian>

#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "serverpool.h"

#include <netinet/in.h> // for ntohs
#include "audiooutput.h"

#include "mythraopdevice.h"
#include "mythraopconnection.h"

#define LOC QString("RAOP Conn: ")
#define MAX_PACKET_SIZE  2048
#define DEFAULT_SAMPLE_RATE 44100

RSA* MythRAOPConnection::g_rsa = NULL;

class NetStream : public QTextStream
{
public:
    NetStream(QIODevice *device) : QTextStream(device)
    {
    };
    NetStream &operator<<(const QString &str)
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            LOC + QString("Sending(%1): ").arg(str.length()) + str);
        QTextStream *q = this;
        *q << str;
        return *this;
    };
};

MythRAOPConnection::MythRAOPConnection(QObject *parent, QTcpSocket *socket,
                                       QByteArray id, int port)
  : QObject(parent), m_watchdogTimer(NULL), m_socket(socket),
    m_textStream(NULL), m_hardwareId(id),
    m_dataPort(port), m_dataSocket(NULL),
    m_clientControlSocket(NULL), m_clientControlPort(0),
    m_audio(NULL), m_codec(NULL), m_codeccontext(NULL),
    m_sampleRate(DEFAULT_SAMPLE_RATE), m_queueLength(0), m_allowVolumeControl(true),
    m_seenPacket(false), m_lastPacketSequence(0), m_lastPacketTimestamp(0),
    m_lastSyncTime(0), m_lastSyncTimestamp(0), m_lastLatency(0), m_latencyAudio(0),
    m_latencyQueued(0), m_latencyCounter(0), m_avSync(0), m_audioTimer(NULL)
{
}

MythRAOPConnection::~MythRAOPConnection()
{
    // delete audio timer
    StopAudioTimer();

    // stop and delete watchdog timer
    if (m_watchdogTimer)
        m_watchdogTimer->stop();
    delete m_watchdogTimer;
    m_watchdogTimer = NULL;

    // delete main socket
    if (m_socket)
    {
        m_socket->close();
        m_socket->deleteLater();
    }
    m_socket = NULL;

    // delete data socket
    if (m_dataSocket)
    {
        m_dataSocket->disconnect();
        m_dataSocket->close();
        m_dataSocket->deleteLater();
    }
    m_dataSocket = NULL;

    // client control socket
    if (m_clientControlSocket)
    {
        m_clientControlSocket->disconnect();
        m_clientControlSocket->close();
        m_clientControlSocket->deleteLater();
    }
    m_clientControlSocket = NULL;

    // close audio decoder
    DestroyDecoder();

    // close audio device
    CloseAudioDevice();

    // free decoded audio buffer
    ExpireAudio(UINT64_MAX);
}

bool MythRAOPConnection::Init(void)
{
    // connect up the request socket
    m_textStream = new NetStream(m_socket);
    m_textStream->setCodec("UTF-8");
    if (!connect(m_socket, SIGNAL(readyRead()), this, SLOT(readClient())))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to connect client socket signal.");
        return false;
    }

    // create the data socket
    m_dataSocket = new ServerPool();
    if (!connect(m_dataSocket, SIGNAL(newDatagram(QByteArray, QHostAddress, quint16)),
                 this,         SLOT(udpDataReady(QByteArray, QHostAddress, quint16))))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to connect data socket signal.");
        return false;
    }

    // try a few ports in case the first is in use
    int baseport = m_dataPort;
    while (m_dataPort < baseport + RAOP_PORT_RANGE)
    {
        if (m_dataSocket->bind(m_dataPort))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Bound to port %1 for incoming data").arg(m_dataPort));
            break;
        }
        m_dataPort++;
    }

    if (m_dataPort >= baseport + RAOP_PORT_RANGE)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to bind to a port for data.");
        return false;
    }

    // load the private key
    if (!LoadKey())
        return false;

    // use internal volume control
    m_allowVolumeControl = gCoreContext->GetNumSetting("MythControlsVolume", 1);

    // start the watchdog timer to auto delete the client after a period of inactivity
    m_watchdogTimer = new QTimer();
    connect(m_watchdogTimer, SIGNAL(timeout()), this, SLOT(timeout()));
    m_watchdogTimer->start(10000);

    return true;
}

void MythRAOPConnection::udpDataReady(void)
{
    QUdpSocket *socket = dynamic_cast<QUdpSocket*>(sender());

    while (socket->state() == QAbstractSocket::BoundState &&
           socket->hasPendingDatagrams())
    {
        QByteArray buffer;
        buffer.resize(socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        socket->readDatagram(buffer.data(), buffer.size(),
                             &sender, &senderPort);
        udpDataReady(buffer, sender, senderPort);
    }
}

void MythRAOPConnection::udpDataReady(QByteArray buf, QHostAddress peer,
                                      quint16 port)
{
    // get the time of day
    // NOTE: the previous code would perform this once, and loop internally
    //       since the new code loops externally, and this must get performed
    //       on each pass, there may be issues
    timeval t;
    gettimeofday(&t, NULL);
    uint64_t timenow = (t.tv_sec * 1000) + (t.tv_usec / 1000);

    // restart the idle timer
    if (m_watchdogTimer)
        m_watchdogTimer->start(10000);

    if (!m_audio || !m_codec || !m_codeccontext)
        return;

    unsigned int type = (unsigned int)((char)(buf[1] & ~0x80));

    if (type == 0x54)
        ProcessSyncPacket(buf, timenow);

    if (!(type == 0x60 || type == 0x56))
        return;

    int offset = type == 0x60 ? 0 : 4;
    uint16_t this_sequence  = ntohs(*(uint16_t *)(buf.data() + offset + 2));
    uint64_t this_timestamp = FramesToMs(ntohl(*(uint64_t*)(buf.data() + offset + 4)));
    uint16_t expected_sequence = m_lastPacketSequence + 1; // should wrap ok

    // regular data packet
    if (type == 0x60)
    {
        if (m_seenPacket && (this_sequence != expected_sequence))
            SendResendRequest(timenow, expected_sequence, this_sequence);

        // don't update the sequence for resends
        m_seenPacket = true;
        m_lastPacketSequence  = this_sequence;
        m_lastPacketTimestamp = this_timestamp;
    }

    // resent packet
    if (type == 0x56)
    {
        if (m_resends.contains(this_sequence))
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Received required resend %1")
                .arg(this_sequence));
            m_resends.remove(this_sequence);
        }
        else
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Received unexpected resent packet %1")
                .arg(this_sequence));
    }

    ExpireResendRequests(timenow);

    offset += 12;
    char* data_in = buf.data() + offset;
    int len       = buf.size() - offset;
    if (len < 16)
        return;

    int aeslen = len & ~0xf;
    unsigned char iv[16];
    unsigned char decrypted_data[MAX_PACKET_SIZE];
    memcpy(iv, m_AESIV.data(), sizeof(iv));
    AES_cbc_encrypt((const unsigned char*)data_in,
                    decrypted_data, aeslen,
                        &m_aesKey, iv, AES_DECRYPT);
    memcpy(decrypted_data + aeslen, data_in + aeslen, len - aeslen);

    AVPacket tmp_pkt;
    AVCodecContext *ctx = m_codeccontext;

    av_init_packet(&tmp_pkt);
    tmp_pkt.data = decrypted_data;
    tmp_pkt.size = len;

    while (tmp_pkt.size > 0)
    {
        int decoded_data_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        int16_t *samples = (int16_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE);
        int ret = avcodec_decode_audio3(ctx, samples,
                                        &decoded_data_size, &tmp_pkt);

        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error decoding audio"));
            break;
        }

        if (decoded_data_size > 0)
        {
            int frames = (ctx->channels <= 0 || decoded_data_size < 0) ? -1 :
                decoded_data_size /
                (ctx->channels * av_get_bits_per_sample_fmt(ctx->sample_fmt)>>3);
            AudioFrame aframe;
            aframe.samples = samples;
            aframe.frames  = frames;
            aframe.size    = decoded_data_size;

            if (m_audioQueue.contains(this_timestamp))
                LOG(VB_GENERAL, LOG_WARNING,
                    LOC + "Duplicate packet timestamp.");

            m_audioQueue.insert(this_timestamp, aframe);
            m_queueLength += aframe.frames;
            ProcessAudio(timenow);

            this_timestamp += (frames * 1000) / m_sampleRate;
        }

        tmp_pkt.data += ret;
        tmp_pkt.size -= ret;
    }
}

uint64_t MythRAOPConnection::FramesToMs(uint64_t timestamp)
{
    return (uint64_t)((double)timestamp * 1000.0 / m_sampleRate);
}

void MythRAOPConnection::SendResendRequest(uint64_t timenow,
                                           uint16_t expected, uint16_t got)
{
    if (!m_clientControlSocket)
        return;

    int16_t missed = (got < expected) ?
                (int16_t)(((int32_t)got + UINT16_MAX + 1) - expected) :
                got - expected;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Missed %1 packet(s): expected %2 got %3")
        .arg(missed).arg(expected).arg(got));

    char req[8];
    req[0] = 0x80;
    req[1] = 0x55 | 0x80;
    *(uint16_t *)(req + 2) = htons(1);
    *(uint16_t *)(req + 4) = htons(expected); // missed seqnum
    *(uint16_t *)(req + 6) = htons(missed); // count

    if (m_clientControlSocket->writeDatagram(req, 8, m_peerAddress, m_clientControlPort) == 8)
    {
        for (uint16_t count = 0; count < missed; count++)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Sent resend for %1")
                .arg(expected + count));
            m_resends.insert(expected + count, timenow);
        }
    }
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to send resend request.");
}

void MythRAOPConnection::ExpireResendRequests(uint64_t timenow)
{
    if (!m_resends.size())
        return;

    uint64_t too_old = timenow - 500;
    QMutableMapIterator<uint16_t,uint64_t> it(m_resends);
    while (it.hasNext())
    {
        it.next();
        if (it.value() < too_old)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Never received resend packet %1").arg(it.key()));
            m_resends.remove(it.key());
        }
    }
}

void MythRAOPConnection::ProcessSyncPacket(const QByteArray &buf, uint64_t timenow)
{
    m_lastSyncTimestamp = FramesToMs(ntohl(*(uint64_t*)(buf.data() + 4)));
    uint64_t now   = FramesToMs(ntohl(*(uint64_t*)(buf.data() + 16)));
    m_lastLatency  = now - m_lastSyncTimestamp;
    m_lastSyncTime = timenow;
    uint64_t averageaudio = 0;
    uint64_t averagequeue = 0;
    double   averageav    = 0;
    if (m_latencyCounter)
    {
        averageaudio = m_latencyAudio / m_latencyCounter;
        averagequeue = m_latencyQueued / m_latencyCounter;
        averageav    = m_avSync / (double)m_latencyCounter;
    }

    if (m_audio)
    {
        uint64_t total = averageaudio + averagequeue;
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Sync packet: Timestamp: %1 Current Audio ts: %2 (avsync %3ms) "
                    "Latency: audio %4 queue %5 total %6ms <-> target %7ms")
            .arg(m_lastSyncTimestamp).arg(m_audio->GetAudiotime()).arg(averageav, 0)
            .arg(averageaudio).arg(averagequeue)
            .arg(total).arg(m_lastLatency));
    }
    m_latencyAudio = m_latencyQueued = m_latencyCounter = m_avSync = 0;
}

int MythRAOPConnection::ExpireAudio(uint64_t timestamp)
{
    int res = 0;
    QMutableMapIterator<uint64_t,AudioFrame> it(m_audioQueue);
    while (it.hasNext())
    {
        it.next();
        if (it.key() < timestamp)
        {
            AudioFrame frame = it.value();
            av_free((void *)frame.samples);
            m_audioQueue.remove(it.key());
            m_queueLength -= frame.frames;
            res++;
        }
    }
    return res;
}

void MythRAOPConnection::ProcessAudio(uint64_t timenow)
{
    if (!m_audio)
    {
        ExpireAudio(UINT64_MAX);
        return;
    }

    uint64_t updatedsync  = m_lastSyncTimestamp + (timenow - m_lastSyncTime);

    // expire anything that is late
    int dumped = ExpireAudio(m_lastSyncTimestamp-m_lastLatency);

    if (dumped > 0)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Dumped %1 audio packets")
            .arg(dumped));
    }

    int64_t avsync        = (int64_t)(m_audio->GetAudiotime() - (int64_t)updatedsync);
    uint64_t queue_length = FramesToMs(m_queueLength);
    uint64_t ideal_ts     = updatedsync - queue_length - avsync + m_lastLatency;

    m_avSync += avsync;
    m_latencyAudio += m_audio->GetAudioBufferedTime();;
    m_latencyQueued += queue_length;
    m_latencyCounter++;

    QMapIterator<uint64_t,AudioFrame> it(m_audioQueue);
    while (it.hasNext())
    {
        it.next();
        if (it.key() < ideal_ts)
        {
            AudioFrame aframe = it.value();
            m_audio->AddData((char *)aframe.samples, aframe.size,
                             it.key(), aframe.frames);
        }
    }

    ExpireAudio(ideal_ts);
}

void MythRAOPConnection::ResetAudio(void)
{
    ExpireAudio(UINT64_MAX);
    m_latencyCounter = m_latencyAudio = m_latencyQueued = m_avSync = 0;
    m_seenPacket = false;
    if (m_audio)
        m_audio->Reset();
}

void MythRAOPConnection::timeout(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Closing connection after inactivity.");
    m_socket->disconnectFromHost();
}

void MythRAOPConnection::audioRetry(void)
{
    if (!m_audio)
    {
        MythRAOPDevice* p = (MythRAOPDevice*)parent();
        if (p && p->NextInAudioQueue(this) && OpenAudioDevice())
            CreateDecoder();
    }

    if (m_audio && m_codec && m_codeccontext)
        StopAudioTimer();
}

void MythRAOPConnection::readClient(void)
{
    QTcpSocket *socket = (QTcpSocket *)sender();
    if (!socket)
        return;

    QList<QByteArray> lines;
    while (socket->canReadLine())
    {
        QByteArray line = socket->readLine();
        lines.append(line);
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "readClient: " + line.trimmed().data());
    }

    if (lines.size())
        ProcessRequest(lines);
}

void MythRAOPConnection::ProcessRequest(const QList<QByteArray> &lines)
{
    if (lines.isEmpty())
        return;

    RawHash tags = FindTags(lines);

    if (!tags.contains("CSeq"))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ProcessRequest: Didn't find CSeq");
        return;
    }

    *m_textStream << "RTSP/1.0 200 OK\r\n";

    if (tags.contains("Apple-Challenge"))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Received Apple-Challenge"));

        *m_textStream << "Apple-Response: ";
        if (!LoadKey())
            return;
        int tosize = RSA_size(LoadKey());
        unsigned char to[tosize];

        QByteArray challenge = QByteArray::fromBase64(tags["Apple-Challenge"].data());
        int challenge_size = challenge.size();
        if (challenge_size != 16)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Decoded challenge size %1, expected 16").arg(challenge_size));
            if (challenge_size > 16)
                challenge_size = 16;
        }

        int i = 0;
        unsigned char from[38];
        memcpy(from, challenge.data(), challenge_size);
        i += challenge_size;
        if (m_socket->localAddress().protocol() == QAbstractSocket::IPv4Protocol)
        {
            uint32_t ip = m_socket->localAddress().toIPv4Address();
            ip = qToBigEndian(ip);
            memcpy(from + i, &ip, 4);
            i += 4;
        }
        else if (m_socket->localAddress().protocol() == QAbstractSocket::IPv6Protocol)
        {
            // NB IPv6 untested
            Q_IPV6ADDR ip = m_socket->localAddress().toIPv6Address();
            //ip = qToBigEndian(ip);
            memcpy(from + i, &ip, 16);
            i += 16;
        }
        memcpy(from + i, m_hardwareId.data(), RAOP_HARDWARE_ID_SIZE);
        i += RAOP_HARDWARE_ID_SIZE;

        int pad = 32 - i;
        if (pad > 0)
        {
            memset(from + i, 0, pad);
            i += pad;
        }

        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Full base64 response: '%1' size %2")
            .arg(QByteArray((const char*)from, i).toBase64().data()).arg(i));

        RSA_private_encrypt(i, from, to, LoadKey(), RSA_PKCS1_PADDING);

        QByteArray base64 = QByteArray((const char*)to, tosize).toBase64();

        for (int pos = base64.size() - 1; pos > 0; pos--)
        {
            if (base64[pos] == '=')
                base64[pos] = ' ';
            else
                break;
        }
        LOG(VB_GENERAL, LOG_DEBUG, QString("tSize=%1 tLen=%2 tResponse=%3")
            .arg(tosize).arg(base64.size()).arg(base64.data()));
        *m_textStream << base64.trimmed() << "\r\n";
    }

    QByteArray option = lines[0].left(lines[0].indexOf(" "));

    if (option == "OPTIONS")
    {
        StartResponse(m_textStream, option, tags["CSeq"]);
        *m_textStream << "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER\r\n";
    }
    else if (option == "ANNOUNCE")
    {
        foreach (QByteArray line, lines)
        {
            if (line.startsWith("a=rsaaeskey:"))
            {
                QByteArray key = line.mid(12).trimmed();
                QByteArray decodedkey = QByteArray::fromBase64(key);
                LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("RSAAESKey: %1 (decoded size %2)")
                    .arg(key.data()).arg(decodedkey.size()));

                if (LoadKey())
                {
                    int size = sizeof(char) * RSA_size(LoadKey());
                    char *decryptedkey = new char[size];
                    if (RSA_private_decrypt(decodedkey.size(),
                                            (const unsigned char*)decodedkey.data(),
                                            (unsigned char*)decryptedkey,
                                            LoadKey(), RSA_PKCS1_OAEP_PADDING))
                    {
                        LOG(VB_GENERAL, LOG_DEBUG, LOC +
                            "Successfully decrypted AES key from RSA.");
                        AES_set_decrypt_key((const unsigned char*)decryptedkey,
                                            128, &m_aesKey);
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            "Failed to decrypt AES key from RSA.");
                    }
                    delete [] decryptedkey;
                }

            }
            else if (line.startsWith("a=aesiv:"))
            {
                QByteArray aesiv = line.mid(8).trimmed();
                m_AESIV = QByteArray::fromBase64(aesiv.data());
                LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("AESIV: %1 (decoded size %2)")
                    .arg(aesiv.data()).arg(m_AESIV.size()));
            }
            else if (line.startsWith("a=fmtp:"))
            {
                m_audioFormat.clear();
                QByteArray format = line.mid(7).trimmed();
                QList<QByteArray> fmts = format.split(' ');
                foreach (QByteArray fmt, fmts)
                    m_audioFormat.append(fmt.toInt());

                foreach (int fmt, m_audioFormat)
                    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Audio parameter: %1").arg(fmt));
            }
        }
        StartResponse(m_textStream, option, tags["CSeq"]);
    }
    else if (option == "SETUP")
    {
        if (tags.contains("Transport"))
        {
            int control_port = 0;
            int timing_port = 0;
            QString data = tags["Transport"];
            QStringList items = data.split(";");
            foreach (QString item, items)
            {
                if (item.startsWith("control_port"))
                    control_port = item.mid(item.indexOf("=") + 1).trimmed().toUInt();
                else if (item.startsWith("timing_port"))
                    timing_port  = item.mid(item.indexOf("=") + 1).trimmed().toUInt();
            }

            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Negotiated setup with client %1 on port %2")
                    .arg(m_socket->peerAddress().toString())
                    .arg(m_socket->peerPort()));
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("control port: %1 timing port: %2")
                    .arg(control_port).arg(timing_port));

            if (m_clientControlSocket)
            {
                m_clientControlSocket->disconnect();
                m_clientControlSocket->close();
                delete m_clientControlSocket;
            }

            m_clientControlSocket = new QUdpSocket(this);
            if (!m_clientControlSocket->bind(control_port))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Failed to bind to client control port %1. "
                            "Control of audio stream may fail")
                    .arg(control_port));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("Bound to client control port %1").arg(control_port));
            }

            if (OpenAudioDevice())
                CreateDecoder();

            m_peerAddress = m_socket->peerAddress();
            m_clientControlPort = control_port;
            connect(m_clientControlSocket, SIGNAL(readyRead()), this, SLOT(udpDataReady()));

            StartResponse(m_textStream, option, tags["CSeq"]);
            *m_textStream << "Transport: " << tags["Transport"].data();
            *m_textStream << ";server_port=" << QString::number(m_dataPort);
            *m_textStream << "\r\nSession: MYTHTV\r\n";
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "No Transport details found - Ignoring");
        }
    }
    else if (option == "RECORD")
    {
        StartResponse(m_textStream, option, tags["CSeq"]);
    }
    else if (option == "TEARDOWN")
    {
        *m_textStream << "Connection: close\r\n";
        StartResponse(m_textStream, option, tags["CSeq"]);
    }
    else if (option == "FLUSH")
    {
        ResetAudio();
        *m_textStream << "flush\r\n";
        StartResponse(m_textStream, option, tags["CSeq"]);
    }
    else if (option == "SET_PARAMETER")
    {
        foreach (QByteArray line, lines)
        {
            StartResponse(m_textStream, option, tags["CSeq"]);
            if (line.startsWith("volume:") && m_allowVolumeControl && m_audio)
            {
                QByteArray rawvol = line.mid(7).trimmed();
                float volume = (rawvol.toFloat() + 30.0) / 0.3;
                if (volume < 0.01)
                    volume = 0.0;
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Setting volume to %1 (raw %3)")
                    .arg(volume).arg(rawvol.data()));
                m_audio->SetCurrentVolume((int)volume);
            }
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Command not handled: %1")
            .arg(option.data()));
        StartResponse(m_textStream, option, tags["CSeq"]);
    }
    FinishResponse(m_textStream, m_socket, option, tags["CSeq"]);
}

void MythRAOPConnection::StartResponse(NetStream *stream,
                                       QByteArray &option, QByteArray &cseq)
{
    if (!stream)
        return;
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("%1 sequence %2")
        .arg(option.data()).arg(cseq.data()));
    *stream << "Audio-Jack-Status: connected; type=analog\r\n";
    *stream << "CSeq: " << cseq << "\r\n";
}

void MythRAOPConnection::FinishResponse(NetStream *stream, QTcpSocket *socket,
                                        QByteArray &option, QByteArray &cseq)
{
    *stream << "\r\n";
    stream->flush();
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Finished %1 %2 , Send: %3")
        .arg(option.data()).arg(cseq.data()).arg(socket->flush()));
}

RSA* MythRAOPConnection::LoadKey(void)
{
    static QMutex lock;
    QMutexLocker locker(&lock);

    if (g_rsa)
        return g_rsa;

    QString sName( "/RAOPKey.rsa" );
    FILE * file = fopen(GetConfDir().toUtf8() + sName.toUtf8(), "rb");

    if ( !file )
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to read key from: %1")
            .arg(GetConfDir() + sName));
        g_rsa = NULL;
        return NULL;
    }

    g_rsa = PEM_read_RSAPrivateKey(file, NULL, NULL, NULL);
    fclose(file);

    if (g_rsa)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Loaded RSA private key (%1)").arg(RSA_check_key(g_rsa)));
        return g_rsa;
    }

    g_rsa = NULL;
    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to load RSA private key.");
    return NULL;
}

RawHash MythRAOPConnection::FindTags(const QList<QByteArray> &lines)
{
    RawHash result;
    if (lines.isEmpty())
        return result;

    for (int i = 0; i < lines.size(); i++)
    {
        int index = lines[i].indexOf(":");
        if (index > 0)
        {
            result.insert(lines[i].left(index).trimmed(),
                          lines[i].mid(index + 1).trimmed());
        }
    }
    return result;
}

bool MythRAOPConnection::CreateDecoder(void)
{
    DestroyDecoder();

    // create an ALAC decoder
    avcodeclock->lock();
    av_register_all();
    avcodeclock->unlock();

    m_codec = avcodec_find_decoder(CODEC_ID_ALAC);
    if (!m_codec)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create ALAC decoder- going silent...");
        return false;
    }

    m_codeccontext = avcodec_alloc_context();
    if (m_codec && m_codeccontext)
    {
        unsigned char* extradata = new unsigned char[36];
        memset(extradata, 0, 36);
        if (m_audioFormat.size() < 12)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Creating decoder but haven't seen audio format.");
        }
        else
        {
            uint32_t fs = m_audioFormat[1]; // frame size
            extradata[12] = (fs >> 24) & 0xff;
            extradata[13] = (fs >> 16) & 0xff;
            extradata[14] = (fs >> 8)  & 0xff;
            extradata[15] = fs & 0xff;
            extradata[16] = m_channels;       // channels
            extradata[17] = m_audioFormat[3]; // sample size
            extradata[18] = m_audioFormat[4]; // rice_historymult
            extradata[19] = m_audioFormat[5]; // rice_initialhistory
            extradata[20] = m_audioFormat[6]; // rice_kmodifier
        }
        m_codeccontext->extradata = extradata;
        m_codeccontext->extradata_size = 36;
        m_codeccontext->channels = m_channels;
        if (avcodec_open(m_codeccontext, m_codec) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to open ALAC decoder - going silent...");
            DestroyDecoder();
            return false;
        }
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Opened ALAC decoder.");
    }

    return true;
}

void MythRAOPConnection::DestroyDecoder(void)
{
    if (m_codeccontext)
    {
        avcodec_close(m_codeccontext);
        av_free(m_codeccontext);
    }
    m_codec = NULL;
    m_codeccontext = NULL;
}

bool MythRAOPConnection::OpenAudioDevice(void)
{
    CloseAudioDevice();

    m_sampleRate = m_audioFormat.size() >= 12 ? m_audioFormat[11] : DEFAULT_SAMPLE_RATE;
    m_channels   = m_audioFormat[7] > 0 ? m_audioFormat[7] : 2;
    if (m_sampleRate < 1)
        m_sampleRate = DEFAULT_SAMPLE_RATE;

    QString passthru = gCoreContext->GetNumSetting("PassThruDeviceOverride", false)
                        ? gCoreContext->GetSetting("PassThruOutputDevice") : QString::null;
    QString device = gCoreContext->GetSetting("AudioOutputDevice");

    m_audio = AudioOutput::OpenAudio(device, passthru, FORMAT_S16, m_channels,
                                     0, m_sampleRate, AUDIOOUTPUT_MUSIC,
                                     m_allowVolumeControl, false);
    if (!m_audio)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to open audio device. Going silent...");
        CloseAudioDevice();
        StartAudioTimer();
        return false;
    }

    QString error = m_audio->GetError();
    if (!error.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Audio not initialised. Message was '%1'")
            .arg(error));
        CloseAudioDevice();
        StartAudioTimer();
        return false;
    }

    StopAudioTimer();
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Opened audio device.");
    return true;
}

void MythRAOPConnection::CloseAudioDevice(void)
{
    delete m_audio;
    m_audio = NULL;
}

void MythRAOPConnection::StartAudioTimer(void)
{
    if (m_audioTimer)
        return;

    m_audioTimer = new QTimer();
    connect(m_audioTimer, SIGNAL(timeout()), this, SLOT(audioRetry()));
    m_audioTimer->start(5000);
}

void MythRAOPConnection::StopAudioTimer(void)
{
    if (m_audioTimer)
        m_audioTimer->stop();
    delete m_audioTimer;
    m_audioTimer = NULL;
}
