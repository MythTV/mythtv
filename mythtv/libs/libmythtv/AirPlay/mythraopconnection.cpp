#include <QTimer>
#include <QTcpSocket>
#include <QtEndian>
#include <QTextStream>

#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "serverpool.h"

#include "audiooutput.h"

#include "mythraopdevice.h"
#include "mythraopconnection.h"
#include "mythairplayserver.h"

#define LOC QString("RAOP Conn: ")
#define MAX_PACKET_SIZE  2048

RSA *MythRAOPConnection::g_rsa = NULL;
QString MythRAOPConnection::g_rsaLastError;

// RAOP RTP packet type
#define TIMING_REQUEST   0x52
#define TIMING_RESPONSE  0x53
#define SYNC             0x54
#define FIRSTSYNC        (0x54 | 0x80)
#define RANGE_RESEND     0x55
#define AUDIO_RESEND     0x56
#define AUDIO_DATA       0x60
#define FIRSTAUDIO_DATA  (0x60 | 0x80)


// Size (in ms) of audio buffered in audio card
#define AUDIOCARD_BUFFER 800
// How frequently we may call ProcessAudio (via QTimer)
// ideally 20ms, but according to documentation
// anything lower than 50ms on windows, isn't reliable
#define AUDIO_BUFFER     100

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
  : QObject(parent),       m_watchdogTimer(NULL),    m_socket(socket),
    m_textStream(NULL),    m_hardwareId(id),
    m_incomingHeaders(),   m_incomingContent(),      m_incomingPartial(false),
    m_incomingSize(0),
    m_dataSocket(NULL),                              m_dataPort(port),
    m_clientControlSocket(NULL),                     m_clientControlPort(0),
    m_clientTimingSocket(NULL),                      m_clientTimingPort(0),
    m_eventServer(NULL),
    m_audio(NULL),         m_codec(NULL),            m_codeccontext(NULL),
    m_channels(2),         m_sampleSize(16),         m_frameRate(44100),
    m_framesPerPacket(352),m_dequeueAudioTimer(NULL),
    m_queueLength(0),      m_streamingStarted(false),
    m_allowVolumeControl(true),
    //audio sync
    m_seqNum(0),
    m_lastSequence(0),     m_lastTimestamp(0),
    m_currentTimestamp(0), m_nextSequence(0),        m_nextTimestamp(0),
    m_bufferLength(0),     m_timeLastSync(0),
    m_cardLatency(0),      m_adjustedLatency(0),     m_audioStarted(false),
    // clock sync
    m_masterTimeStamp(0),  m_deviceTimeStamp(0),     m_networkLatency(0),
    m_clockSkew(0),
    m_audioTimer(NULL),
    m_progressStart(0),    m_progressCurrent(0),     m_progressEnd(0)
{
}

MythRAOPConnection::~MythRAOPConnection()
{
    CleanUp();

    foreach (QTcpSocket *client, m_eventClients)
    {
        client->close();
        client->deleteLater();
    }
    m_eventClients.clear();

    if (m_eventServer)
    {
        m_eventServer->disconnect();
        m_eventServer->close();
        m_eventServer->deleteLater();
        m_eventServer = NULL;
    }

    // delete main socket
    if (m_socket)
    {
        m_socket->disconnect();
        m_socket->close();
        m_socket->deleteLater();
        m_socket = NULL;
    }

    if (m_textStream)
    {
        delete m_textStream;
        m_textStream = NULL;
    }
}

void MythRAOPConnection::CleanUp(void)
{
    // delete audio timer
    StopAudioTimer();

    // stop and delete watchdog timer
    if (m_watchdogTimer)
    {
        m_watchdogTimer->stop();
        delete m_watchdogTimer;
        m_watchdogTimer = NULL;
    }

    if (m_dequeueAudioTimer)
    {
        m_dequeueAudioTimer->stop();
        delete m_dequeueAudioTimer;
        m_dequeueAudioTimer = NULL;
    }

    if (m_clientTimingSocket)
    {
        m_clientTimingSocket->disconnect();
        m_clientTimingSocket->close();
        delete m_clientTimingSocket;
        m_clientTimingSocket = NULL;
    }

    // delete data socket
    if (m_dataSocket)
    {
        m_dataSocket->disconnect();
        m_dataSocket->close();
        m_dataSocket->deleteLater();
        m_dataSocket = NULL;
    }

    // client control socket
    if (m_clientControlSocket)
    {
        m_clientControlSocket->disconnect();
        m_clientControlSocket->close();
        m_clientControlSocket->deleteLater();
        m_clientControlSocket = NULL;
    }

    // close audio decoder
    DestroyDecoder();

    // free decoded audio buffer
    ResetAudio();

    // close audio device
    CloseAudioDevice();
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
    m_dataPort = m_dataSocket->tryBindingPort(m_dataPort, RAOP_PORT_RANGE);
    if (m_dataPort < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to bind to a port for data.");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Bound to port %1 for incoming data").arg(m_dataPort));

    // load the private key
    if (!LoadKey())
        return false;

    // use internal volume control
    m_allowVolumeControl = gCoreContext->GetNumSetting("MythControlsVolume", 1);

    // start the watchdog timer to auto delete the client after a period of inactivity
    m_watchdogTimer = new QTimer();
    connect(m_watchdogTimer, SIGNAL(timeout()), this, SLOT(timeout()));
    m_watchdogTimer->start(10000);

    m_dequeueAudioTimer = new QTimer();
    connect(m_dequeueAudioTimer, SIGNAL(timeout()), this, SLOT(ProcessAudio()));

    return true;
}

/**
 * Socket incoming data signal handler
 * use for audio, control and timing socket
 */
void MythRAOPConnection::udpDataReady(QByteArray buf, QHostAddress peer,
                                      quint16 port)
{
    // restart the idle timer
    if (m_watchdogTimer)
        m_watchdogTimer->start(10000);

    if (!m_audio || !m_codec || !m_codeccontext)
        return;

    uint8_t  type;
    uint16_t seq;
    uint64_t timestamp;

    if (!GetPacketType(buf, type, seq, timestamp))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Packet doesn't start with valid Rtp Header (0x%1)")
            .arg((uint8_t)buf[0], 0, 16));
        return;
    }

    switch (type)
    {
        case SYNC:
        case FIRSTSYNC:
            ProcessSync(buf);
            ProcessAudio();
            return;

        case FIRSTAUDIO_DATA:
            m_nextSequence  = seq;
            m_nextTimestamp = timestamp;
            // With iTunes we know what the first sequence is going to be.
            // iOS device do not tell us before streaming start what the first
            // packet is going to be.
            m_streamingStarted = true;
            break;

        case AUDIO_DATA:
        case AUDIO_RESEND:
            break;

        case TIMING_RESPONSE:
            ProcessTimeResponse(buf);
            return;

        default:
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Packet type (0x%1) not handled")
                .arg(type, 0, 16));
            return;
    }

    timestamp = framesToMs(timestamp);
    if (timestamp < m_currentTimestamp)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Received packet %1 too late, ignoring")
            .arg(seq));
        return;
    }
    // regular data packet
    if (type == AUDIO_DATA || type == FIRSTAUDIO_DATA)
    {
        if (m_streamingStarted && seq != m_nextSequence)
            SendResendRequest(timestamp, m_nextSequence, seq);

        m_nextSequence     = seq + 1;
        m_nextTimestamp    = timestamp;
        m_streamingStarted = true;
    }

    if (!m_streamingStarted)
        return;

    // resent packet
    if (type == AUDIO_RESEND)
    {
        if (m_resends.contains(seq))
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Received required resend %1 (with ts:%2 last:%3)")
                .arg(seq).arg(timestamp).arg(m_nextSequence));
            m_resends.remove(seq);
        }
        else
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Received unexpected resent packet %1")
                .arg(seq));
    }

    // Check that the audio packet is valid, do so by decoding it. If an error
    // occurs, ask to resend it
    QList<AudioData> *decoded = new QList<AudioData>();
    int numframes = decodeAudioPacket(type, &buf, decoded);
    if (numframes < 0)
    {
        // an error occurred, ask for the audio packet once again.
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error decoding audio"));
        SendResendRequest(timestamp, seq, seq+1);
        return;
    }
    AudioPacket frames;
    frames.seq    = seq;
    frames.data   = decoded;
    m_audioQueue.insert(timestamp, frames);
    ProcessAudio();
}

void MythRAOPConnection::ProcessSync(const QByteArray &buf)
{
    bool    first       = (uint8_t)buf[0] == 0x90; // First sync is 0x90,0x55
    const char *req     = buf.constData();
    uint64_t current_ts = qFromBigEndian(*(uint32_t *)(req + 4));
    uint64_t next_ts    = qFromBigEndian(*(uint32_t *)(req + 16));

    uint64_t current    = framesToMs(current_ts);
    uint64_t next       = framesToMs(next_ts);

    m_currentTimestamp  = current;
    m_nextTimestamp     = next;
    m_bufferLength      = m_nextTimestamp - m_currentTimestamp;

    if (first)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Receiving first SYNC packet"));
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Receiving SYNC packet"));
    }

    timeval  t; gettimeofday(&t, NULL);
    m_timeLastSync = t.tv_sec * 1000 + t.tv_usec / 1000;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("SYNC: cur:%1 next:%2 time:%3")
        .arg(m_currentTimestamp).arg(m_nextTimestamp).arg(m_timeLastSync));

    uint64_t delay = framesToMs(m_audioQueue.size() * m_framesPerPacket);
    delay += m_networkLatency;

    // Calculate audio card latency
    if (first)
    {
        m_cardLatency = AudioCardLatency();
        // if audio isn't started, start playing 200ms worth of silence
        // and measure timestamp difference
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Audio hardware latency: %1ms").arg(m_cardLatency));
    }

    uint64_t audiots = m_audio->GetAudiotime();
    if (m_audioStarted)
    {
        m_adjustedLatency = (int64_t)audiots - (int64_t)m_currentTimestamp;
    }
    if (m_adjustedLatency > (int64_t)m_bufferLength)
    {
        // Too much delay in playback
        // will reset audio card in next ProcessAudio
        m_audioStarted = false;
        m_adjustedLatency = 0;
    }

    delay += m_audio->GetAudioBufferedTime();
    delay += m_adjustedLatency;

    // Expire old audio
    ExpireResendRequests(m_currentTimestamp);
    int res = ExpireAudio(m_currentTimestamp);
    if (res > 0)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Drop %1 packets").arg(res));
    }

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Queue=%1 buffer=%2ms ideal=%3ms diffts:%4ms")
        .arg(m_audioQueue.size())
        .arg(delay)
        .arg(m_bufferLength)
        .arg(m_adjustedLatency));
}

/**
 * SendResendRequest:
 * Request RAOP client to resend missed RTP packets
 */
void MythRAOPConnection::SendResendRequest(uint64_t timestamp,
                                           uint16_t expected, uint16_t got)
{
    if (!m_clientControlSocket)
        return;

    int16_t missed = (got < expected) ?
                (int16_t)(((int32_t)got + UINT16_MAX + 1) - expected) :
                got - expected;

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Missed %1 packet(s): expected %2 got %3 ts:%4")
        .arg(missed).arg(expected).arg(got).arg(timestamp));

    char req[8];
    req[0] = 0x80;
    req[1] = RANGE_RESEND | 0x80;
    *(uint16_t *)(req + 2) = qToBigEndian(m_seqNum++);
    *(uint16_t *)(req + 4) = qToBigEndian(expected);   // missed seqnum
    *(uint16_t *)(req + 6) = qToBigEndian(missed);     // count

    if (m_clientControlSocket->writeDatagram(req, sizeof(req),
                                             m_peerAddress, m_clientControlPort)
        == sizeof(req))
    {
        for (uint16_t count = 0; count < missed; count++)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Sent resend for %1")
                .arg(expected + count));
            m_resends.insert(expected + count, timestamp);
        }
    }
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to send resend request.");
}

/**
 * ExpireResendRequests:
 * Expire resend requests that are older than timestamp. Those requests are
 * expired when audio with older timestamp has already been played
 */
void MythRAOPConnection::ExpireResendRequests(uint64_t timestamp)
{
    if (!m_resends.size())
        return;

    QMutableMapIterator<uint16_t,uint64_t> it(m_resends);
    while (it.hasNext())
    {
        it.next();
        if (it.value() < timestamp && m_streamingStarted)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Never received resend packet %1").arg(it.key()));
            m_resends.remove(it.key());
        }
    }
}

/**
 * SendTimeRequest:
 * Send a time request to the RAOP client.
 */
void MythRAOPConnection::SendTimeRequest(void)
{
    if (!m_clientControlSocket) // should never happen
        return;

    timeval t;
    gettimeofday(&t, NULL);

    char req[32];
    req[0] = 0x80;
    req[1] = TIMING_REQUEST | 0x80;
    // this is always 0x00 0x07 according to http://blog.technologeek.org/airtunes-v2
    // no other value works
    req[2] = 0x00;
    req[3] = 0x07;
    *(uint32_t *)(req + 4)  = (uint32_t)0;
    *(uint64_t *)(req + 8)  = (uint64_t)0;
    *(uint64_t *)(req + 16) = (uint64_t)0;
    *(uint32_t *)(req + 24) = qToBigEndian((uint32_t)t.tv_sec);
    *(uint32_t *)(req + 28) = qToBigEndian((uint32_t)t.tv_usec);

    if (m_clientTimingSocket->writeDatagram(req, sizeof(req), m_peerAddress, m_clientTimingPort) != sizeof(req))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to send resend time request.");
        return;
    }
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Requesting master time (Local %1.%2)")
        .arg(t.tv_sec).arg(t.tv_usec));
}

/**
 * ProcessTimeResponse:
 * Calculate the network latency, we do not use the reference time send by itunes
 * instead we measure the time lapsed between the request and the response
 * the latency is calculated in ms
 */
void MythRAOPConnection::ProcessTimeResponse(const QByteArray &buf)
{
    timeval t1, t2;
    const char *req = buf.constData();

    t1.tv_sec  = qFromBigEndian(*(uint32_t *)(req + 8));
    t1.tv_usec = qFromBigEndian(*(uint32_t *)(req + 12));

    gettimeofday(&t2, NULL);
    uint64_t time1, time2;
    time1 = t1.tv_sec * 1000 + t1.tv_usec / 1000;
    time2 = t2.tv_sec * 1000 + t2.tv_usec / 1000;
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Read back time (Local %1.%2)")
        .arg(t1.tv_sec).arg(t1.tv_usec));
    // network latency equal time difference in ms between request and response
    // divide by two for approximate time of one way trip
    m_networkLatency = (time2 - time1) / 2;

    // now calculate the time difference between the client and us.
    // this is NTP time, where sec is in seconds, and ticks is in 1/2^32s
    uint32_t sec    = qFromBigEndian(*(uint32_t *)(req + 24));
    uint32_t ticks  = qFromBigEndian(*(uint32_t *)(req + 28));
    // convert ticks into ms
    int64_t  master = NTPToLocal(sec, ticks);
    m_clockSkew     = master - time2;
}

uint64_t MythRAOPConnection::NTPToLocal(uint32_t sec, uint32_t ticks)
{
    return (int64_t)sec * 1000LL + (((int64_t)ticks * 1000LL) >> 32);
}

bool MythRAOPConnection::GetPacketType(const QByteArray &buf, uint8_t &type,
                                       uint16_t &seq, uint64_t &timestamp)
{
    // All RAOP packets start with | 0x80/0x90 (first sync) | PACKET_TYPE |
    if ((uint8_t)buf[0] != 0x80 && (uint8_t)buf[0] != 0x90)
    {
        return false;
    }

    type = (char)buf[1];
    // Is it first sync packet?
    if ((uint8_t)buf[0] == 0x90 && type == FIRSTSYNC)
    {
        return true;
    }
    if (type != FIRSTAUDIO_DATA)
    {
        type &= ~0x80;
    }

    if (type != AUDIO_DATA && type != FIRSTAUDIO_DATA && type != AUDIO_RESEND)
        return true;

    const char *ptr = buf.constData();
    if (type == AUDIO_RESEND)
    {
        ptr += 4;
    }
    seq  = qFromBigEndian(*(uint16_t *)(ptr + 2));
    timestamp = qFromBigEndian(*(uint32_t *)(ptr + 4));
    return true;
}

// Audio decode / playback related routines

uint32_t MythRAOPConnection::decodeAudioPacket(uint8_t type,
                                               const QByteArray *buf,
                                               QList<AudioData> *dest)
{
    const char *data_in = buf->constData();
    int len             = buf->size();
    if (type == AUDIO_RESEND)
    {
        data_in += 4;
        len     -= 4;
    }
    data_in += 12;
    len     -= 12;
    if (len < 16)
        return -1;

    int aeslen = len & ~0xf;
    unsigned char iv[16];
    unsigned char decrypted_data[MAX_PACKET_SIZE];
    memcpy(iv, m_AESIV.constData(), sizeof(iv));
    AES_cbc_encrypt((const unsigned char *)data_in,
                    decrypted_data, aeslen,
                    &m_aesKey, iv, AES_DECRYPT);
    memcpy(decrypted_data + aeslen, data_in + aeslen, len - aeslen);

    AVPacket tmp_pkt;
    AVCodecContext *ctx = m_codeccontext;

    av_init_packet(&tmp_pkt);
    tmp_pkt.data = decrypted_data;
    tmp_pkt.size = len;

    uint32_t frames_added = 0;
    while (tmp_pkt.size > 0)
    {
        uint8_t *samples = (uint8_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE);
        AVFrame frame;
        int got_frame = 0;

        int ret = avcodec_decode_audio4(ctx, &frame, &got_frame, &tmp_pkt);

        if (ret < 0)
        {
            av_free(samples);
            return -1;
        }

        if (got_frame)
        {
            // ALAC codec isn't planar
            int data_size = av_samples_get_buffer_size(NULL, ctx->channels,
                                                       frame.nb_samples,
                                                       ctx->sample_fmt, 1);
            memcpy(samples, frame.extended_data[0], data_size);

            frames_added += frame.nb_samples;
            AudioData block;
            block.data    = samples;
            block.length  = data_size;
            block.frames  = frame.nb_samples;
            dest->append(block);
        }
        tmp_pkt.data += ret;
        tmp_pkt.size -= ret;
    }
    return frames_added;
}

void MythRAOPConnection::ProcessAudio()
{
    if (!m_streamingStarted || !m_audio)
        return;

    if (m_audio->IsPaused())
    {
        // ALSA takes a while to unpause, enough to have SYNC starting to drop
        // packets, so unpause as early as possible
        m_audio->Pause(false);
    }
    timeval  t; gettimeofday(&t, NULL);
    uint64_t dtime    = (t.tv_sec * 1000 + t.tv_usec / 1000) - m_timeLastSync;
    uint64_t rtp      = dtime + m_currentTimestamp + m_networkLatency;
    uint64_t buffered = m_audioStarted ? m_audio->GetAudioBufferedTime() : 0;

    // Keep audio framework buffer as short as possible, keeping everything in
    // m_audioQueue, so we can easily reset the least amount possible
    if (buffered > AUDIOCARD_BUFFER)
        return;

    // Also make sure m_audioQueue never goes to less than 1/3 of the RDP stream
    // total latency, this should gives us enough time to receive missed packets
    uint64_t queue = framesToMs(m_audioQueue.size() * m_framesPerPacket);
    if (queue < m_bufferLength / 3)
        return;

    rtp += buffered;
    rtp += m_cardLatency;

    // How many packets to add to the audio card, to fill AUDIOCARD_BUFFER
    int max_packets    = ((AUDIOCARD_BUFFER - buffered)
                          * m_frameRate / 1000) / m_framesPerPacket;
    int i              = 0;
    uint64_t timestamp = 0;

    QMapIterator<uint64_t,AudioPacket> packet_it(m_audioQueue);
    while (packet_it.hasNext() && i <= max_packets)
    {
        packet_it.next();

        timestamp = packet_it.key();
        if (timestamp < rtp)
        {
            if (!m_audioStarted)
            {
                m_audio->Reset(); // clear audio card
            }
            AudioPacket frames = packet_it.value();

            if (m_lastSequence != frames.seq)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Audio discontinuity seen. Played %1 (%3) expected %2")
                    .arg(frames.seq).arg(m_lastSequence).arg(timestamp));
                m_lastSequence = frames.seq;
            }
            m_lastSequence++;

            QList<AudioData>::iterator it = frames.data->begin();
            for (; it != frames.data->end(); ++it)
            {
                AudioData *data = &(*it);
                m_audio->AddData((char *)data->data, data->length,
                                 timestamp, data->frames);
                timestamp += m_audio->LengthLastData();
            }
            i++;
            m_audioStarted = true;
        }
        else // QMap is sorted, so no need to continue if not found
            break;
    }

    ExpireAudio(timestamp);
    m_lastTimestamp = timestamp;

    // restart audio timer should we stop receiving data on regular interval,
    // we need to continue processing the audio queue
    m_dequeueAudioTimer->start(AUDIO_BUFFER);
}

int MythRAOPConnection::ExpireAudio(uint64_t timestamp)
{
    int res = 0;
    QMutableMapIterator<uint64_t,AudioPacket> packet_it(m_audioQueue);
    while (packet_it.hasNext())
    {
        packet_it.next();
        if (packet_it.key() < timestamp)
        {
            AudioPacket frames = packet_it.value();
            if (frames.data)
            {
                QList<AudioData>::iterator it = frames.data->begin();
                for (; it != frames.data->end(); ++it)
                {
                    av_free(it->data);
                }
                delete frames.data;
            }
            m_audioQueue.remove(packet_it.key());
            res++;
        }
    }
    return res;
}

void MythRAOPConnection::ResetAudio(void)
{
    if (m_audio)
    {
        m_audio->Reset();
    }
    ExpireAudio(UINT64_MAX);
    ExpireResendRequests(UINT64_MAX);
    m_audioStarted = false;
}

void MythRAOPConnection::timeout(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Closing connection after inactivity.");
    m_socket->disconnectFromHost();
}

void MythRAOPConnection::audioRetry(void)
{
    if (!m_audio && OpenAudioDevice())
    {
        CreateDecoder();
    }

    if (m_audio && m_codec && m_codeccontext)
    {
        StopAudioTimer();
    }
}

/**
 * readClient: signal handler for RAOP client connection
 * Handle initialisation of session
 */
void MythRAOPConnection::readClient(void)
{
    QTcpSocket *socket = (QTcpSocket *)sender();
    if (!socket)
        return;

    QByteArray data = socket->readAll();
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("readClient(%1): ")
        .arg(data.size()) + data.constData());

    // For big content, we may be called several times for a single packet
    if (!m_incomingPartial)
    {
        m_incomingHeaders.clear();
        m_incomingContent.clear();
        m_incomingSize = 0;

        QTextStream stream(data);
        QString line;
        do
        {
            line = stream.readLine();
            if (line.size() == 0)
                break;
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Header(%1) = %2")
                .arg(m_socket->peerAddress().toString())
                .arg(line));
            m_incomingHeaders.append(line);
            if (line.contains("Content-Length:"))
            {
                m_incomingSize = line.mid(line.indexOf(" ") + 1).toInt();
            }
        }
        while (!line.isNull());

        if (m_incomingHeaders.size() == 0)
            return;

        if (!stream.atEnd())
        {
            int pos = stream.pos();
            if (pos > 0)
            {
                m_incomingContent.append(data.mid(pos));
            }
        }
    }
    else
    {
        m_incomingContent.append(data);
    }

    // If we haven't received all the content yet, wait (see when receiving
    // coverart
    if (m_incomingContent.size() < m_incomingSize)
    {
        m_incomingPartial = true;
        return;
    }
    else
    {
        m_incomingPartial = false;
    }
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Content(%1) = %2")
        .arg(m_incomingContent.size()).arg(m_incomingContent.constData()));

    ProcessRequest(m_incomingHeaders, m_incomingContent);
}

void MythRAOPConnection::ProcessRequest(const QStringList &header,
                                        const QByteArray &content)
{
    if (header.isEmpty())
        return;

    RawHash tags = FindTags(header);

    if (!tags.contains("CSeq"))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ProcessRequest: Didn't find CSeq");
        return;
    }

    QString option = header[0].left(header[0].indexOf(" "));

    // process RTP-info field
    bool gotRTP = false;
    uint16_t RTPseq;
    uint64_t RTPtimestamp;
    if (tags.contains("RTP-Info"))
    {
        gotRTP = true;
        QString data = tags["RTP-Info"];
        QStringList items = data.split(";");
        foreach (QString item, items)
        {
            if (item.startsWith("seq"))
            {
                RTPseq = item.mid(item.indexOf("=") + 1).trimmed().toUShort();
            }
            else if (item.startsWith("rtptime"))
            {
                RTPtimestamp = item.mid(item.indexOf("=") + 1).trimmed().toUInt();
            }
        }
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("RTP-Info: seq=%1 rtptime=%2")
            .arg(RTPseq).arg(RTPtimestamp));
    }

    if (gCoreContext->GetNumSetting("AirPlayPasswordEnabled", false))
    {
        if (m_nonce.isEmpty())
        {
            m_nonce = GenerateNonce();
        }
        if (!tags.contains("Authorization"))
        {
            // 60 seconds to enter password.
            m_watchdogTimer->start(60000);
            FinishAuthenticationResponse(m_textStream, m_socket, tags["CSeq"]);
            return;
        }

        QByteArray auth;
        if (DigestMd5Response(tags["Authorization"], option, m_nonce,
                              gCoreContext->GetSetting("AirPlayPassword"),
                              auth) == auth)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "RAOP client authenticated");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "RAOP authentication failed");
            FinishAuthenticationResponse(m_textStream, m_socket, tags["CSeq"]);
            return;
        }
    }
    *m_textStream << "RTSP/1.0 200 OK\r\n";

    if (tags.contains("Apple-Challenge"))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Received Apple-Challenge"));
        
        *m_textStream << "Apple-Response: ";
        if (!LoadKey())
            return;
        int tosize = RSA_size(LoadKey());
        uint8_t *to = new uint8_t[tosize];
        
        QByteArray challenge =
        QByteArray::fromBase64(tags["Apple-Challenge"].toAscii());
        int challenge_size = challenge.size();
        if (challenge_size != 16)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Decoded challenge size %1, expected 16")
                .arg(challenge_size));
            if (challenge_size > 16)
                challenge_size = 16;
        }
        
        int i = 0;
        unsigned char from[38];
        memcpy(from, challenge.constData(), challenge_size);
        i += challenge_size;
        if (m_socket->localAddress().protocol() ==
            QAbstractSocket::IPv4Protocol)
        {
            uint32_t ip = m_socket->localAddress().toIPv4Address();
            ip = qToBigEndian(ip);
            memcpy(from + i, &ip, 4);
            i += 4;
        }
        else if (m_socket->localAddress().protocol() ==
                 QAbstractSocket::IPv6Protocol)
        {
            Q_IPV6ADDR ip = m_socket->localAddress().toIPv6Address();
            if(memcmp(&ip,
                      "\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\xff\xff",
                      12) == 0)
            {
                memcpy(from + i, &ip[12], 4);
                i += 4;
            }
            else
            {
                memcpy(from + i, &ip, 16);
                i += 16;
            }
        }
        memcpy(from + i, m_hardwareId.constData(), AIRPLAY_HARDWARE_ID_SIZE);
        i += AIRPLAY_HARDWARE_ID_SIZE;
        
        int pad = 32 - i;
        if (pad > 0)
        {
            memset(from + i, 0, pad);
            i += pad;
        }
        
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Full base64 response: '%1' size %2")
            .arg(QByteArray((const char *)from, i).toBase64().constData())
            .arg(i));
        
        RSA_private_encrypt(i, from, to, LoadKey(), RSA_PKCS1_PADDING);
        
        QByteArray base64 = QByteArray((const char *)to, tosize).toBase64();
        delete[] to;
        
        for (int pos = base64.size() - 1; pos > 0; pos--)
        {
            if (base64[pos] == '=')
                base64[pos] = ' ';
            else
                break;
        }
        LOG(VB_GENERAL, LOG_DEBUG, QString("tSize=%1 tLen=%2 tResponse=%3")
            .arg(tosize).arg(base64.size()).arg(base64.constData()));
        *m_textStream << base64.trimmed() << "\r\n";
    }

    if (option == "OPTIONS")
    {
        *m_textStream << "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, "
            "TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER, POST, GET\r\n";
    }
    else if (option == "ANNOUNCE")
    {
        QStringList lines = splitLines(content);
        foreach (QString line, lines)
        {
            if (line.startsWith("a=rsaaeskey:"))
            {
                QString key = line.mid(12).trimmed();
                QByteArray decodedkey = QByteArray::fromBase64(key.toAscii());
                LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    QString("RSAAESKey: %1 (decoded size %2)")
                    .arg(key).arg(decodedkey.size()));

                if (LoadKey())
                {
                    int size = sizeof(char) * RSA_size(LoadKey());
                    char *decryptedkey = new char[size];
                    if (RSA_private_decrypt(decodedkey.size(),
                                            (const unsigned char *)decodedkey.constData(),
                                            (unsigned char *)decryptedkey,
                                            LoadKey(), RSA_PKCS1_OAEP_PADDING))
                    {
                        LOG(VB_GENERAL, LOG_DEBUG, LOC +
                            "Successfully decrypted AES key from RSA.");
                        AES_set_decrypt_key((const unsigned char *)decryptedkey,
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
                QString aesiv = line.mid(8).trimmed();
                m_AESIV = QByteArray::fromBase64(aesiv.toAscii());
                LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    QString("AESIV: %1 (decoded size %2)")
                    .arg(aesiv).arg(m_AESIV.size()));
            }
            else if (line.startsWith("a=fmtp:"))
            {
                m_audioFormat.clear();
                QString format = line.mid(7).trimmed();
                QList<QString> fmts = format.split(' ');
                foreach (QString fmt, fmts)
                    m_audioFormat.append(fmt.toInt());

                foreach (int fmt, m_audioFormat)
                    LOG(VB_GENERAL, LOG_DEBUG, LOC +
                        QString("Audio parameter: %1").arg(fmt));
                m_framesPerPacket = m_audioFormat[1];
                m_sampleSize      = m_audioFormat[3];
                m_channels        = m_audioFormat[7];
                m_frameRate       = m_audioFormat[11];
            }
        }
    }
    else if (option == "SETUP")
    {
        if (tags.contains("Transport"))
        {
            // New client is trying to play audio, disconnect all the other clients
            ((MythRAOPDevice*)parent())->DeleteAllClients(this);
            gCoreContext->WantingPlayback(parent());

            int control_port = 0;
            int timing_port = 0;
            QString data = tags["Transport"];
            QStringList items = data.split(";");
            bool events = false;

            foreach (QString item, items)
            {
                if (item.startsWith("control_port"))
                    control_port = item.mid(item.indexOf("=") + 1).trimmed().toUInt();
                else if (item.startsWith("timing_port"))
                    timing_port  = item.mid(item.indexOf("=") + 1).trimmed().toUInt();
                else if (item.startsWith("events"))
                    events = true;
            }

            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Negotiated setup with client %1 on port %2")
                    .arg(m_socket->peerAddress().toString())
                    .arg(m_socket->peerPort()));
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("control port: %1 timing port: %2")
                    .arg(control_port).arg(timing_port));

            m_peerAddress = m_socket->peerAddress();

            if (m_clientControlSocket)
            {
                m_clientControlSocket->disconnect();
                m_clientControlSocket->close();
                delete m_clientControlSocket;
            }

            m_clientControlSocket = new ServerPool(this);
            int controlbind_port =
                m_clientControlSocket->tryBindingPort(control_port,
                                                      RAOP_PORT_RANGE);
            if (controlbind_port < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Failed to bind to client control port. "
                            "Control of audio stream may fail"));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("Bound to client control port %1 on port %2")
                    .arg(control_port).arg(controlbind_port));
            }
            m_clientControlPort = control_port;
            connect(m_clientControlSocket,
                    SIGNAL(newDatagram(QByteArray, QHostAddress, quint16)),
                    this,
                    SLOT(udpDataReady(QByteArray, QHostAddress, quint16)));

            if (m_clientTimingSocket)
            {
                m_clientTimingSocket->disconnect();
                m_clientTimingSocket->close();
                delete m_clientTimingSocket;
            }

            m_clientTimingSocket = new ServerPool(this);
            int timingbind_port =
                m_clientTimingSocket->tryBindingPort(timing_port,
                                                     RAOP_PORT_RANGE);
            if (timingbind_port < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Failed to bind to client timing port. "
                            "Timing of audio stream will be incorrect"));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("Bound to client timing port %1 on port %2")
                    .arg(timing_port).arg(timingbind_port));
            }
            m_clientTimingPort = timing_port;
            connect(m_clientTimingSocket,
                    SIGNAL(newDatagram(QByteArray, QHostAddress, quint16)),
                    this,
                    SLOT(udpDataReady(QByteArray, QHostAddress, quint16)));

            if (m_eventServer)
            {
                // Should never get here, but just in case
                QTcpSocket *client;
                foreach (client, m_eventClients)
                {
                    client->disconnect();
                    client->abort();
                    delete client;
                }
                m_eventClients.clear();
                m_eventServer->disconnect();
                m_eventServer->close();
                delete m_eventServer;
                m_eventServer = NULL;
            }

            if (events)
            {
                m_eventServer = new ServerPool(this);
                m_eventPort = m_eventServer->tryListeningPort(timing_port,
                                                              RAOP_PORT_RANGE);
                if (m_eventPort < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "Failed to find a port for RAOP events server.");
                }
                else
                {
                    LOG(VB_GENERAL, LOG_INFO, LOC +
                        QString("Listening for RAOP events on port %1").arg(m_eventPort));
                    connect(m_eventServer, SIGNAL(newConnection(QTcpSocket *)),
                            this, SLOT(newEventClient(QTcpSocket *)));
                }
            }

            if (OpenAudioDevice())
                CreateDecoder();

            // Recreate transport line with new ports value
            QString newdata;
            bool first = true;
            foreach (QString item, items)
            {
                if (!first)
                {
                    newdata += ";";
                }
                if (item.startsWith("control_port"))
                {
                    newdata += "control_port=" + QString::number(controlbind_port);
                }
                else if (item.startsWith("timing_port"))
                {
                    newdata += "timing_port=" + QString::number(timingbind_port);
                }
                else if (item.startsWith("events"))
                {
                    newdata += "event_port=" + QString::number(m_eventPort);
                }
                else
                {
                    newdata += item;
                }
                first = false;
            }
            if (!first)
            {
                newdata += ";";
            }
            newdata += "server_port=" + QString::number(m_dataPort);

            *m_textStream << "Transport: " << newdata << "\r\n";
            *m_textStream << "Session: 1\r\n";
            *m_textStream << "Audio-Jack-Status: connected\r\n";
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "No Transport details found - Ignoring");
        }
    }
    else if (option == "RECORD")
    {
        if (gotRTP)
        {
            m_nextSequence  = RTPseq;
            m_nextTimestamp = RTPtimestamp;
        }
        // Ask for master clock value to determine time skew and average network latency
        SendTimeRequest();
    }
    else if (option == "FLUSH")
    {
        if (gotRTP)
        {
            m_nextSequence     = RTPseq;
            m_nextTimestamp    = RTPtimestamp;
            m_currentTimestamp = m_nextTimestamp - m_bufferLength;
        }
        // determine RTP timestamp of last sample played
        uint64_t timestamp = m_audioStarted && m_audio ?
            m_audio->GetAudiotime() : m_lastTimestamp;
        *m_textStream << "RTP-Info: rtptime=" << QString::number(timestamp);
        m_streamingStarted = false;
        ResetAudio();
    }
    else if (option == "SET_PARAMETER")
    {
        if (tags.contains("Content-Type"))
        {
            if (tags["Content-Type"] == "text/parameters")
            {
                QString name  = content.left(content.indexOf(":"));
                QString param = content.mid(content.indexOf(":") + 1).trimmed();

                LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    QString("text/parameters: name=%1 parem=%2")
                    .arg(name).arg(param));

                if (name == "volume" && m_allowVolumeControl && m_audio)
                {
                    float volume = (param.toFloat() + 30.0f) * 100.0f / 30.0f;
                    if (volume < 0.01f)
                        volume = 0.0f;
                    LOG(VB_GENERAL, LOG_INFO,
                        LOC + QString("Setting volume to %1 (raw %3)")
                        .arg(volume).arg(param));
                    m_audio->SetCurrentVolume((int)volume);
                }
                else if (name == "progress")
                {
                    QStringList items = param.split("/");
                    if (items.size() == 3)
                    {
                        m_progressStart   = items[0].toUInt();
                        m_progressCurrent = items[1].toUInt();
                        m_progressEnd     = items[2].toUInt();
                    }
                    int length  =
                        (m_progressEnd-m_progressStart) / m_frameRate;
                    int current =
                        (m_progressCurrent-m_progressStart) / m_frameRate;

                    LOG(VB_GENERAL, LOG_INFO,
                        LOC +QString("Progress: %1/%2")
                        .arg(stringFromSeconds(current))
                        .arg(stringFromSeconds(length)));
                }
            }
            else if(tags["Content-Type"] == "image/jpeg")
            {
                // Receiving image coverart
                m_artwork = content;
            }
            else if (tags["Content-Type"] == "application/x-dmap-tagged")
            {
                // Receiving DMAP metadata
                QMap<QString,QString> map = decodeDMAP(content);
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Receiving Title:%1 Artist:%2 Album:%3 Format:%4")
                    .arg(map["minm"]).arg(map["asar"])
                    .arg(map["asal"]).arg(map["asfm"]));
            }
        }
    }
    else if (option == "TEARDOWN")
    {
        m_socket->disconnectFromHost();
        return;
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Command not handled: %1")
            .arg(option));
    }
    FinishResponse(m_textStream, m_socket, option, tags["CSeq"]);
}

void MythRAOPConnection::FinishAuthenticationResponse(NetStream *stream,
                                                      QTcpSocket *socket,
                                                      QString &cseq)
{
    if (!stream)
        return;
    *stream << "RTSP/1.0 401 Unauthorised\r\n";
    *stream << "Server: AirTunes/130.14\r\n";
    *stream << "WWW-Authenticate: Digest realm=\"raop\", ";
    *stream << "nonce=\"" + m_nonce + "\"\r\n";
    *stream << "CSeq: " << cseq << "\r\n";
    *stream << "\r\n";
    stream->flush();
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Finished Authentication request %2, Send: %3")
        .arg(cseq).arg(socket->flush()));
}

void MythRAOPConnection::FinishResponse(NetStream *stream, QTcpSocket *socket,
                                        QString &option, QString &cseq)
{
    if (!stream)
        return;
    *stream << "Server: AirTunes/130.14\r\n";
    *stream << "CSeq: " << cseq << "\r\n";
    *stream << "\r\n";
    stream->flush();
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Finished %1 %2 , Send: %3")
        .arg(option).arg(cseq).arg(socket->flush()));
}

/**
 * LoadKey. Load RSA key into static variable for re-using it later
 * The RSA key is resident in memory for the entire duration of the application
 * as such RSA_free is never called on it.
 */
RSA *MythRAOPConnection::LoadKey(void)
{
    static QMutex lock;
    QMutexLocker locker(&lock);

    if (g_rsa)
        return g_rsa;

    QString sName( "/RAOPKey.rsa" );
    FILE *file = fopen(GetConfDir().toUtf8() + sName.toUtf8(), "rb");

    if ( !file )
    {
        g_rsaLastError = QObject::tr("Failed to read key from: %1").arg(GetConfDir() + sName);
        g_rsa = NULL;
        LOG(VB_GENERAL, LOG_ERR, LOC + g_rsaLastError);
        return NULL;
    }

    g_rsa = PEM_read_RSAPrivateKey(file, NULL, NULL, NULL);
    fclose(file);

    if (g_rsa)
    {
        g_rsaLastError = "";
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Loaded RSA private key (%1)").arg(RSA_check_key(g_rsa)));
        return g_rsa;
    }

    g_rsaLastError = QObject::tr("Failed to load RSA private key.");
    g_rsa = NULL;
    LOG(VB_GENERAL, LOG_ERR, LOC + g_rsaLastError);
    return NULL;
}

RawHash MythRAOPConnection::FindTags(const QStringList &lines)
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

QStringList MythRAOPConnection::splitLines(const QByteArray &lines)
{
    QStringList list;
    QTextStream stream(lines);

    QString line;
    do
    {
        line = stream.readLine();
        if (!line.isNull())
        {
            list.append(line);
        }
    }
    while (!line.isNull());

    return list;
}

/**
 * stringFromSeconds:
 *
 * Usage: stringFromSeconds(seconds)
 * Description: create a string in the format HH:mm:ss from a duration in seconds
 * HH: will not be displayed if there's less than one hour
 */
QString MythRAOPConnection::stringFromSeconds(int time)
{
    int   hour    = time / 3600;
    int   minute  = (time - hour * 3600) / 60;
    int seconds   = time - hour * 3600 - minute * 60;
    QString str;

    if (hour)
    {
        str += QString("%1:").arg(hour);
    }
    if (minute < 10)
    {
        str += "0";
    }
    str += QString("%1:").arg(minute);
    if (seconds < 10)
    {
        str += "0";
    }
    str += QString::number(seconds);
    return str;
}

/**
 * framesDuration
 * Description: return the duration in ms of frames
 *
 */
uint64_t MythRAOPConnection::framesToMs(uint64_t frames)
{
    return (frames * 1000ULL) / m_frameRate;
}

/**
 * decodeDMAP:
 *
 * Usage: decodeDMAP(QByteArray &dmap)
 * Description: decode the DMAP (Digital Media Access Protocol) object.
 * The object returned is a map of the dmap tags and their associated content
 */
QMap<QString,QString> MythRAOPConnection::decodeDMAP(const QByteArray &dmap)
{
    QMap<QString,QString> result;
    int offset = 8;
    while (offset < dmap.size())
    {
        QString tag = dmap.mid(offset, 4);
        offset += 4;
        uint32_t length = qFromBigEndian(*(uint32_t *)(dmap.constData() + offset));
        offset += sizeof(uint32_t);
        QString content = QString::fromUtf8(dmap.mid(offset,
                                                     length).constData());
        offset += length;
        result.insert(tag, content);
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
        LOG(VB_GENERAL, LOG_ERR, LOC
            + "Failed to create ALAC decoder- going silent...");
        return false;
    }

    m_codeccontext = avcodec_alloc_context3(m_codec);
    if (m_codeccontext)
    {
        unsigned char *extradata = new unsigned char[36];
        memset(extradata, 0, 36);
        if (m_audioFormat.size() < 12)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Creating decoder but haven't seen audio format.");
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
        if (avcodec_open2(m_codeccontext, m_codec, NULL) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to open ALAC decoder - going silent...");
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

    QString passthru = gCoreContext->GetNumSetting("PassThruDeviceOverride", false)
                        ? gCoreContext->GetSetting("PassThruOutputDevice") : QString::null;
    QString device = gCoreContext->GetSetting("AudioOutputDevice");

    m_audio = AudioOutput::OpenAudio(device, passthru, FORMAT_S16, m_channels,
                                     0, m_frameRate, AUDIOOUTPUT_MUSIC,
                                     m_allowVolumeControl, false);
    if (!m_audio)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to open audio device. Going silent...");
        CloseAudioDevice();
        StartAudioTimer();
        return false;
    }

    QString error = m_audio->GetError();
    if (!error.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Audio not initialised. Message was '%1'")
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
    {
        m_audioTimer->stop();
    }
    delete m_audioTimer;
    m_audioTimer = NULL;
}

/**
 * AudioCardLatency:
 * Description: Play silence and calculate audio latency between input / output
 */
int64_t MythRAOPConnection::AudioCardLatency(void)
{
    if (!m_audio)
        return 0;

    uint64_t timestamp = 123456;

    int16_t *samples = (int16_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE);
    int frames = AUDIOCARD_BUFFER * m_frameRate / 1000;
    m_audio->AddData((char *)samples,
                     frames * (m_sampleSize>>3) * m_channels,
                     timestamp,
                     frames);
    av_free(samples);
    usleep(AUDIOCARD_BUFFER * 1000);
    uint64_t audiots = m_audio->GetAudiotime();
    return (int64_t)timestamp - (int64_t)audiots;
}

void MythRAOPConnection::newEventClient(QTcpSocket *client)
{
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("New connection from %1:%2 for RAOP events server.")
        .arg(client->peerAddress().toString()).arg(client->peerPort()));

    m_eventClients.append(client);
    connect(client, SIGNAL(disconnected()), this, SLOT(deleteEventClient()));
    return;
}

void MythRAOPConnection::deleteEventClient(void)
{
    QTcpSocket *client = static_cast<QTcpSocket *>(sender());

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("%1:%2 disconnected from RAOP events server.")
        .arg(client->peerAddress().toString()).arg(client->peerPort()));
}
