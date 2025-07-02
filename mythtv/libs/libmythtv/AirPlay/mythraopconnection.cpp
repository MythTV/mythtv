#include <unistd.h> // for usleep()

#include <algorithm>
#include <limits> // workaround QTBUG-90395
#include <utility>

#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QtEndian>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif

#include "libmythbase/mythlogging.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/serverpool.h"

#include "libmyth/audio/audiooutput.h"
#include "libmyth/audio/audiooutpututil.h"

#include "mythraopdevice.h"
#include "mythraopconnection.h"
#include "mythairplayserver.h"
#include "libmythbase/mythchrono.h"

#include "libmythui/mythmainwindow.h"

// OpenSSL 3.0.0 release
// 8 bits major, 8 bits minor 24 bits patch, 4 bits release flag.
#if OPENSSL_VERSION_NUMBER < 0x030000000L
#define EVP_PKEY_get_id EVP_PKEY_id
#define EVP_PKEY_get_size EVP_PKEY_size
#endif

#define LOC QString("RAOP Conn: ")
static constexpr size_t MAX_PACKET_SIZE { 2048 };

EVP_PKEY *MythRAOPConnection::g_devPrivKey = nullptr;
QString MythRAOPConnection::g_rsaLastError;

// RAOP RTP packet type
static constexpr uint8_t TIMING_REQUEST   { 0x52 };
static constexpr uint8_t TIMING_RESPONSE  { 0x53 };
static constexpr uint8_t SYNC             { 0x54 };
static constexpr uint8_t FIRSTSYNC        { 0x54 | 0x80 };
static constexpr uint8_t RANGE_RESEND     { 0x55 };
static constexpr uint8_t AUDIO_RESEND     { 0x56 };
static constexpr uint8_t AUDIO_DATA       { 0x60 };
static constexpr uint8_t FIRSTAUDIO_DATA  { 0x60 | 0x80 };

// Size (in ms) of audio buffered in audio card
static constexpr std::chrono::milliseconds AUDIOCARD_BUFFER { 500ms };
// How frequently we may call ProcessAudio (via QTimer)
// ideally 20ms, but according to documentation
// anything lower than 50ms on windows, isn't reliable
static constexpr std::chrono::milliseconds AUDIO_BUFFER { 100ms };

class RaopNetStream : public QTextStream
{
public:
    explicit RaopNetStream(QIODevice *device) : QTextStream(device)
    {
    };
    RaopNetStream &operator<<(const QString &str)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG,
            LOC + QString("Sending(%1): ").arg(str.length()) + str.trimmed());
        QTextStream *q = this;
        *q << str;
        return *this;
    };
};

MythRAOPConnection::MythRAOPConnection(QObject *parent, QTcpSocket *socket,
                                       QByteArray id, int port)
  : QObject(parent),
    m_socket(socket),
    m_hardwareId(std::move(id)),
    m_dataPort(port),
    m_cctx(EVP_CIPHER_CTX_new()),
    m_id(GetNotificationCenter()->Register(this))
{
#if OPENSSL_VERSION_NUMBER < 0x030000000L
    m_cipher = EVP_aes_128_cbc();
#else
    //NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
    m_cipher = EVP_CIPHER_fetch(nullptr, "AES-128-CBC", nullptr);
#endif
}

MythRAOPConnection::~MythRAOPConnection()
{
    CleanUp();

    for (QTcpSocket *client : std::as_const(m_eventClients))
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
        m_eventServer = nullptr;
    }

    // delete main socket
    if (m_socket)
    {
        m_socket->disconnect();
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    if (m_textStream)
    {
        delete m_textStream;
        m_textStream = nullptr;
    }

    if (m_id > 0)
    {
        auto *nc = GetNotificationCenter();
        if (nc)
            nc->UnRegister(this, m_id);
    }

    EVP_CIPHER_CTX_free(m_cctx);
    m_cctx = nullptr;
#if OPENSSL_VERSION_NUMBER >= 0x030000000L
    EVP_CIPHER_free(m_cipher);
#endif
    m_cipher = nullptr;
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
        m_watchdogTimer = nullptr;
    }

    if (m_dequeueAudioTimer)
    {
        m_dequeueAudioTimer->stop();
        delete m_dequeueAudioTimer;
        m_dequeueAudioTimer = nullptr;
    }

    if (m_clientTimingSocket)
    {
        m_clientTimingSocket->disconnect();
        m_clientTimingSocket->close();
        delete m_clientTimingSocket;
        m_clientTimingSocket = nullptr;
    }

    // delete data socket
    if (m_dataSocket)
    {
        m_dataSocket->disconnect();
        m_dataSocket->close();
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
    }

    // client control socket
    if (m_clientControlSocket)
    {
        m_clientControlSocket->disconnect();
        m_clientControlSocket->close();
        m_clientControlSocket->deleteLater();
        m_clientControlSocket = nullptr;
    }

    // close audio decoder
    DestroyDecoder();

    // free decoded audio buffer
    ResetAudio();

    // close audio device
    CloseAudioDevice();
    // Tell listeners we're done
    if (m_playbackStarted)
    {
        gCoreContext->emitTVPlaybackStopped();
    }
}

bool MythRAOPConnection::Init(void)
{
    // connect up the request socket
    m_textStream = new RaopNetStream(m_socket);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_textStream->setCodec("UTF-8");
#else
    m_textStream->setEncoding(QStringConverter::Utf8);
#endif
    if (!connect(m_socket, &QIODevice::readyRead, this, &MythRAOPConnection::readClient))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "Failed to connect client socket signal.");
        return false;
    }

    // create the data socket
    m_dataSocket = new ServerPool();
    if (!connect(m_dataSocket, &ServerPool::newDatagram,
                 this,         &MythRAOPConnection::udpDataReady))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "Failed to connect data socket signal.");
        return false;
    }

    // try a few ports in case the first is in use
    m_dataPort = m_dataSocket->tryBindingPort(m_dataPort, RAOP_PORT_RANGE);
    if (m_dataPort < 0)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "Failed to bind to a port for data.");
        return false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Bound to port %1 for incoming data").arg(m_dataPort));

    // load the private key
    if (!LoadKey())
        return false;

    // use internal volume control
    m_allowVolumeControl = gCoreContext->GetBoolSetting("MythControlsVolume", true);

    // start the watchdog timer to auto delete the client after a period of inactivity
    m_watchdogTimer = new QTimer();
    connect(m_watchdogTimer, &QTimer::timeout, this, &MythRAOPConnection::timeout);
    m_watchdogTimer->start(10s);

    m_dequeueAudioTimer = new QTimer();
    connect(m_dequeueAudioTimer, &QTimer::timeout, this, &MythRAOPConnection::ProcessAudio);

    return true;
}

/**
 * Socket incoming data signal handler
 * use for audio, control and timing socket
 */
void MythRAOPConnection::udpDataReady(QByteArray buf, const QHostAddress& /*peer*/,
                                      quint16 /*port*/)
{
    // restart the idle timer
    if (m_watchdogTimer)
        m_watchdogTimer->start(10s);

    if (!m_audio || !m_codec || !m_codecContext)
        return;

    uint8_t  type = 0;
    uint16_t seq = 0;
    uint64_t firstframe = 0;

    if (!GetPacketType(buf, type, seq, firstframe))
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
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
            m_nextSequence      = seq;
            m_nextTimestamp     = framesToMs(firstframe);
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
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                QString("Packet type (0x%1) not handled")
                .arg(type, 0, 16));
            return;
    }

    auto timestamp = framesToMs(firstframe);
    if (timestamp < m_currentTimestamp)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Received packet %1 too late, ignoring")
            .arg(seq));
        return;
    }
    // regular data packet
    if (type == AUDIO_DATA || type == FIRSTAUDIO_DATA)
    {
        if (m_streamingStarted && seq != m_nextSequence)
            SendResendRequest(timestamp, m_nextSequence, seq);

        m_nextSequence      = seq + 1;
        m_nextTimestamp     = timestamp;
        m_streamingStarted  = true;
    }

    if (!m_streamingStarted)
        return;

    // resent packet
    if (type == AUDIO_RESEND)
    {
        if (m_resends.contains(seq))
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                QString("Received required resend %1 (with ts:%2 last:%3)")
                .arg(seq).arg(firstframe).arg(m_nextSequence));
            m_resends.remove(seq);
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                QString("Received unexpected resent packet %1")
                .arg(seq));
        }
    }

    // Check that the audio packet is valid, do so by decoding it. If an error
    // occurs, ask to resend it
    auto *decoded = new QList<AudioData>();
    int numframes = decodeAudioPacket(type, &buf, decoded);
    if (numframes < 0)
    {
        // an error occurred, ask for the audio packet once again.
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Error decoding audio"));
        SendResendRequest(timestamp, seq, seq+1);
        delete decoded;
        return;
    }
    AudioPacket frames { seq, decoded };
    m_audioQueue.insert(timestamp, frames);
    ProcessAudio();
}

void MythRAOPConnection::ProcessSync(const QByteArray &buf)
{
    bool    first       = (uint8_t)buf[0] == 0x90; // First sync is 0x90,0x54
    const char *req     = buf.constData();
    uint64_t current_ts = qFromBigEndian(*(uint32_t *)(req + 4));
    uint64_t next_ts    = qFromBigEndian(*(uint32_t *)(req + 16));

    m_currentTimestamp  = framesToMs(current_ts);
    m_nextTimestamp     = framesToMs(next_ts);
    m_bufferLength      = m_nextTimestamp - m_currentTimestamp;

    if (current_ts > m_progressStart)
    {
        m_progressCurrent = next_ts;
        SendNotification(true);
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Receiving %1SYNC packet")
        .arg(first ? "first " : ""));

    m_timeLastSync = nowAsDuration<std::chrono::milliseconds>();

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("SYNC: cur:%1 next:%2 time:%3")
        .arg(m_currentTimestamp.count()).arg(m_nextTimestamp.count())
        .arg(m_timeLastSync.count()));

    std::chrono::milliseconds delay = framesToMs((uint64_t)m_audioQueue.size() * m_framesPerPacket);
    std::chrono::milliseconds audiots = m_audio->GetAudiotime();
    std::chrono::milliseconds currentLatency = 0ms;

    if (m_audioStarted)
    {
        currentLatency = audiots - m_currentTimestamp;
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("RAOP timestamps: about to play:%1 desired:%2 latency:%3")
        .arg(audiots.count()).arg(m_currentTimestamp.count())
        .arg(currentLatency.count()));

    delay += m_audio->GetAudioBufferedTime();
    delay += currentLatency;

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("Queue=%1 buffer=%2ms ideal=%3ms diffts:%4ms")
        .arg(m_audioQueue.size())
        .arg(delay.count())
        .arg(m_bufferLength.count())
        .arg((m_bufferLength-delay).count()));

    if (m_adjustedLatency <= 0ms && m_audioStarted &&
        (-currentLatency > AUDIOCARD_BUFFER))
    {
        // Too much delay in playback.
        // The threshold is a value chosen to be loose enough so it doesn't
        // trigger too often, but should be low enough to detect any accidental
        // interruptions.
        // Will drop some frames in next ProcessAudio
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Too much delay (%1ms), adjusting")
            .arg((m_bufferLength - delay).count()));

        m_adjustedLatency = m_cardLatency + m_networkLatency;

        // Expire old audio
        ExpireResendRequests(m_currentTimestamp - m_adjustedLatency);
        int res = ExpireAudio(m_currentTimestamp - m_adjustedLatency);
        if (res > 0)
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Drop %1 packets").arg(res));
        }

        m_audioStarted = false;
    }
}

/**
 * SendResendRequest:
 * Request RAOP client to resend missed RTP packets
 */
void MythRAOPConnection::SendResendRequest(std::chrono::milliseconds timestamp,
                                           uint16_t expected, uint16_t got)
{
    if (!m_clientControlSocket)
        return;

    int16_t missed = (got < expected) ?
                (int16_t)(((int32_t)got + UINT16_MAX + 1) - expected) :
                got - expected;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Missed %1 packet(s): expected %2 got %3 ts:%4")
        .arg(missed).arg(expected).arg(got).arg(timestamp.count()));

    std::array<uint8_t,8> req { 0x80, RANGE_RESEND | 0x80};
    *(uint16_t *)(&req[2]) = qToBigEndian(m_seqNum++);
    *(uint16_t *)(&req[4]) = qToBigEndian(expected);   // missed seqnum
    *(uint16_t *)(&req[6]) = qToBigEndian(missed);     // count

    if (m_clientControlSocket->writeDatagram((char *)req.data(), req.size(),
                                             m_peerAddress, m_clientControlPort)
        == (qint64)req.size())
    {
        for (uint16_t count = 0; count < missed; count++)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Sent resend for %1")
                .arg(expected + count));
            m_resends.insert(expected + count, timestamp);
        }
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "Failed to send resend request.");
    }
}

/**
 * ExpireResendRequests:
 * Expire resend requests that are older than timestamp. Those requests are
 * expired when audio with older timestamp has already been played
 */
void MythRAOPConnection::ExpireResendRequests(std::chrono::milliseconds timestamp)
{
    if (m_resends.isEmpty())
        return;

    QMutableMapIterator<uint16_t,std::chrono::milliseconds> it(m_resends);
    while (it.hasNext())
    {
        it.next();
        if (it.value() < timestamp && m_streamingStarted)
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC +
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

    uint32_t ntpSec {0};
    uint32_t ntpTicks {0};
    auto usecs = nowAsDuration<std::chrono::microseconds>();
    microsecondsToNTP(usecs, ntpSec, ntpTicks);

    std::array<uint8_t,32> req {
        0x80, TIMING_REQUEST | 0x80,
        // this is always 0x00 0x07 according to http://blog.technologeek.org/airtunes-v2
        // no other value works
        0x00, 0x07
    };
    *(uint32_t *)(&req[24]) = qToBigEndian(ntpSec);
    *(uint32_t *)(&req[28]) = qToBigEndian(ntpTicks);

    if (m_clientTimingSocket->writeDatagram((char *)req.data(), req.size(), m_peerAddress,
                                            m_clientTimingPort) != (qint64)req.size())
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "Failed to send resend time request.");
        return;
    }
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("Requesting master time (Local %1.%2)")
        .arg(ntpSec,8,16,QChar('0')).arg(ntpTicks,8,16,QChar('0')));
}

/**
 * ProcessTimeResponse:
 * Calculate the network latency, we do not use the reference time send by itunes
 * instead we measure the time lapsed between the request and the response
 * the latency is calculated in ms
 */
void MythRAOPConnection::ProcessTimeResponse(const QByteArray &buf)
{
    const char *req = buf.constData();

    uint32_t ntpSec   = qFromBigEndian(*(uint32_t *)(req + 8));
    uint32_t ntpTicks = qFromBigEndian(*(uint32_t *)(req + 12));
    auto time1 = NTPToLocal(ntpSec, ntpTicks);
    auto time2 = nowAsDuration<std::chrono::milliseconds>();
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Read back time (Local %1.%2)")
        .arg(ntpSec,8,16,QChar('0')).arg(ntpTicks,8,16,QChar('0')));
    // network latency equal time difference in ms between request and response
    // divide by two for approximate time of one way trip
    m_networkLatency = (time2 - time1) / 2;
    LOG(VB_AUDIO, LOG_DEBUG, LOC + QString("Network Latency: %1ms")
        .arg(m_networkLatency.count()));

    // now calculate the time difference between the client and us.
    // this is NTP time, where sec is in seconds, and ticks is in 1/2^32s
    uint32_t sec    = qFromBigEndian(*(uint32_t *)(req + 24));
    uint32_t ticks  = qFromBigEndian(*(uint32_t *)(req + 28));
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Source NTP clock time %1.%2")
        .arg(sec,8,16,QChar('0')).arg(ticks,8,16,QChar('0')));

    // convert ticks into ms
    std::chrono::milliseconds master = NTPToLocal(sec, ticks);

    // This value is typically huge (~50 years) and meaningless as
    // Apple products send uptime, not wall time.
    m_clockSkew     = master - time2;
}


// Timestamps in Airplay packets are stored using the format designed
// for NTP: a 32-bit seconds field and a 32-bit fractional seconds
// field, based on a zero value meaning January 1st 1900. This
// constant handles the conversion between that epoch base, and the
// unix epoch base time of January 1st, 1970.
static constexpr uint64_t CLOCK_EPOCH {0x83aa7e80};
std::chrono::milliseconds MythRAOPConnection::NTPToLocal(uint32_t sec, uint32_t ticks)
{
    return std::chrono::milliseconds((((int64_t)sec - CLOCK_EPOCH) * 1000LL) +
                                     (((int64_t)ticks * 1000LL) >> 32));
}
void MythRAOPConnection::microsecondsToNTP(std::chrono::microseconds usec,
                                           uint32_t &ntpSec, uint32_t &ntpTicks)
{
    ntpSec = duration_cast<std::chrono::seconds>(usec).count() + CLOCK_EPOCH;
    ntpTicks = ((usec % 1s).count() << 32) / 1000000;
}
void MythRAOPConnection::timevalToNTP(timeval t, uint32_t &ntpSec, uint32_t &ntpTicks)
{
    auto micros = durationFromTimeval<std::chrono::microseconds>(t);
    microsecondsToNTP(micros, ntpSec, ntpTicks);
}


///
/// \param buf       A pointer to the received data.
/// \param type      The type of this packet.
/// \param seq       The sequence number of this packet. This value is not
///                  always set.
/// \param timestamp The frame number of the first frame in this
///                  packet.  NOT milliseconds. This value is not
///                  always set.
/// \returns true if the packet header was successfully parsed.
bool MythRAOPConnection::GetPacketType(const QByteArray &buf, uint8_t &type,
                                       uint16_t &seq, uint64_t &timestamp)
{
    // All RAOP packets start with | 0x80/0x90 (first sync) | PACKET_TYPE |
    if ((uint8_t)buf[0] != 0x80 && (uint8_t)buf[0] != 0x90)
    {
        return false;
    }

    type = buf[1];
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
    int outlen1 {0};
    int outlen2 {0};
    std::array<uint8_t,MAX_PACKET_SIZE> decrypted_data {};
    EVP_CIPHER_CTX_reset(m_cctx);
    EVP_CIPHER_CTX_set_padding(m_cctx, 0);
    if (
#if OPENSSL_VERSION_NUMBER < 0x030000000L
        // Debian < 12, Fedora < 36, RHEL < 9, SuSe 15, Ubuntu < 2204
        EVP_DecryptInit_ex(m_cctx, m_cipher, nullptr, m_sessionKey.data(),
                           reinterpret_cast<const uint8_t *>(m_aesIV.data()))
#else
        EVP_DecryptInit_ex2(m_cctx, m_cipher, m_sessionKey.data(),
                            reinterpret_cast<const uint8_t *>(m_aesIV.data()),
                            nullptr)
#endif
        != 1)
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            QString("EVP_DecryptInit_ex failed. (%1)")
            .arg(ERR_error_string(ERR_get_error(), nullptr)));
    }
    else if (EVP_DecryptUpdate(m_cctx, decrypted_data.data(), &outlen1,
                               reinterpret_cast<const uint8_t *>(data_in),
                               aeslen) != 1)
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            QString("EVP_DecryptUpdate failed. (%1)")
            .arg(ERR_error_string(ERR_get_error(), nullptr)));
    }
    else if (EVP_DecryptFinal_ex(m_cctx, decrypted_data.data() + outlen1, &outlen2) != 1)
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            QString("EVP_DecryptFinal_ex failed. (%1)")
            .arg(ERR_error_string(ERR_get_error(), nullptr)));
    }
    std::copy(data_in + aeslen, data_in + len,
              decrypted_data.data() + aeslen);

    AVCodecContext *ctx = m_codecContext;
    AVPacket *tmp_pkt = av_packet_alloc();
    if (tmp_pkt == nullptr)
        return -1;
    tmp_pkt->data = decrypted_data.data();
    tmp_pkt->size = len;

    uint32_t frames_added = 0;
    auto *samples = (uint8_t *)av_mallocz(AudioOutput::kMaxSizeBuffer);
    while (tmp_pkt->size > 0)
    {
        int data_size = 0;
        int ret = AudioOutputUtil::DecodeAudio(ctx, samples,
                                               data_size, tmp_pkt);
        if (ret < 0)
        {
            av_free(samples);
            return -1;
        }

        if (data_size)
        {
            int num_samples = data_size /
                (ctx->ch_layout.nb_channels * av_get_bytes_per_sample(ctx->sample_fmt));

            frames_added += num_samples;
            AudioData block {samples, data_size, num_samples};
            dest->append(block);
        }
        tmp_pkt->data += ret;
        tmp_pkt->size -= ret;
    }
    av_packet_free(&tmp_pkt);
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
    auto     dtime    = nowAsDuration<std::chrono::milliseconds>() - m_timeLastSync;
    auto     rtp      = dtime + m_currentTimestamp;
    auto     buffered = m_audioStarted ? m_audio->GetAudioBufferedTime() : 0ms;

    // Keep audio framework buffer as short as possible, keeping everything in
    // m_audioQueue, so we can easily reset the least amount possible
    if (buffered > AUDIOCARD_BUFFER)
        return;

    // Also make sure m_audioQueue never goes to less than 1/3 of the RDP stream
    // total latency, this should gives us enough time to receive missed packets
    std::chrono::milliseconds queue = framesToMs((uint64_t)m_audioQueue.size() * m_framesPerPacket);
    if (queue < m_bufferLength / 3)
        return;

    rtp += buffered;

    // How many packets to add to the audio card, to fill AUDIOCARD_BUFFER
    int max_packets    = ((AUDIOCARD_BUFFER - buffered).count()
                          * m_frameRate / 1000) / m_framesPerPacket;
    int i              = 0;
    std::chrono::milliseconds timestamp = 0ms;

    QMapIterator<std::chrono::milliseconds,AudioPacket> packet_it(m_audioQueue);
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
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("Audio discontinuity seen. Played %1 (%3) expected %2")
                    .arg(frames.seq).arg(m_lastSequence).arg(timestamp.count()));
                m_lastSequence = frames.seq;
            }
            m_lastSequence++;

            for (const auto & data : std::as_const(*frames.data))
            {
                int offset = 0;
                int framecnt = 0;

                if (m_adjustedLatency > 0ms)
                {
                        // calculate how many frames we have to drop to catch up
                    offset = (m_adjustedLatency.count() * m_frameRate / 1000) *
                        m_audio->GetBytesPerFrame();
                    offset = std::min(offset, data.length);
                    framecnt = offset / m_audio->GetBytesPerFrame();
                    m_adjustedLatency -= framesToMs(framecnt+1);
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                        QString("ProcessAudio: Dropping %1 frames to catch up "
                                "(%2ms to go)")
                        .arg(framecnt).arg(m_adjustedLatency.count()));
                    timestamp += framesToMs(framecnt);
                }
                m_audio->AddData((char *)data.data + offset,
                                 data.length - offset,
                                 std::chrono::milliseconds(timestamp), framecnt);
                timestamp += m_audio->LengthLastData();
            }
            i++;
            m_audioStarted = true;
        }
        else
        {
            // QMap is sorted, so no need to continue if not found
            break;
        }
    }

    ExpireAudio(timestamp);
    m_lastTimestamp = timestamp;

    // restart audio timer should we stop receiving data on regular interval,
    // we need to continue processing the audio queue
    m_dequeueAudioTimer->start(AUDIO_BUFFER);
}

int MythRAOPConnection::ExpireAudio(std::chrono::milliseconds timestamp)
{
    int res = 0;
    QMutableMapIterator<std::chrono::milliseconds,AudioPacket> packet_it(m_audioQueue);
    while (packet_it.hasNext())
    {
        packet_it.next();
        if (packet_it.key() < timestamp)
        {
            AudioPacket frames = packet_it.value();
            if (frames.data)
            {
                for (const auto & data : std::as_const(*frames.data))
                    av_free(data.data);
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
    ExpireAudio(std::chrono::milliseconds::max());
    ExpireResendRequests(std::chrono::milliseconds::max());
    m_audioStarted = false;
}

void MythRAOPConnection::timeout(void)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Closing connection after inactivity.");
    m_socket->disconnectFromHost();
}

void MythRAOPConnection::audioRetry(void)
{
    if (!m_audio && OpenAudioDevice())
    {
        CreateDecoder();
    }

    if (m_audio && m_codec && m_codecContext)
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
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    QByteArray data = socket->readAll();
    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
    {
        QByteArray printable = data.left(32);
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("readClient(%1): %2%3")
            .arg(data.size())
            .arg(printable.toHex().toUpper().data(),
                 data.size() > 32 ? "..." : ""));
    }
    // For big content, we may be called several times for a single packet
    if (!m_incomingPartial)
    {
        m_incomingHeaders.clear();
        m_incomingContent.clear();
        m_incomingSize = 0;

        QTextStream stream(data);
        QString line = stream.readLine();
        while (!line.isEmpty())
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Header(%1) = %2")
                .arg(m_socket->peerAddress().toString(), line));
            m_incomingHeaders.append(line);
            if (line.contains("Content-Length:"))
            {
                m_incomingSize = line.mid(line.indexOf(" ") + 1).toInt();
            }
            line = stream.readLine();
        }

        if (m_incomingHeaders.empty())
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
    m_incomingPartial = false;
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Content(%1) = %2")
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
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "ProcessRequest: Didn't find CSeq");
        return;
    }

    QString option = header[0].left(header[0].indexOf(" "));

    // process RTP-info field
    bool gotRTP = false;
    uint16_t RTPseq = 0;
    uint64_t RTPtimestamp = 0;
    if (tags.contains("RTP-Info"))
    {
        gotRTP = true;
        QString data = tags["RTP-Info"];
        QStringList items = data.split(";");
        for (const QString& item : std::as_const(items))
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
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("RTP-Info: seq=%1 rtptime=%2")
            .arg(RTPseq).arg(RTPtimestamp));
    }

    if (gCoreContext->GetBoolSetting("AirPlayPasswordEnabled", false))
    {
        if (m_nonce.isEmpty())
        {
            m_nonce = GenerateNonce();
        }
        if (!tags.contains("Authorization"))
        {
            // 60 seconds to enter password.
            m_watchdogTimer->start(1min);
            FinishAuthenticationResponse(m_textStream, m_socket, tags["CSeq"]);
            return;
        }

        QByteArray auth;
        if (DigestMd5Response(tags["Authorization"], option, m_nonce,
                              gCoreContext->GetSetting("AirPlayPassword"),
                              auth) == auth)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "RAOP client authenticated");
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "RAOP authentication failed");
            FinishAuthenticationResponse(m_textStream, m_socket, tags["CSeq"]);
            return;
        }
    }
    *m_textStream << "RTSP/1.0 200 OK\r\n";

    if (tags.contains("Apple-Challenge"))
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Received Apple-Challenge"));

        *m_textStream << "Apple-Response: ";
        if (!LoadKey())
            return;
        size_t tosize = EVP_PKEY_size(g_devPrivKey);
        std::vector<uint8_t>to;
        to.resize(tosize);

        QByteArray challenge =
        QByteArray::fromBase64(tags["Apple-Challenge"].toLatin1());
        int challenge_size = challenge.size();
        if (challenge_size != 16)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                QString("Base64 decoded challenge size %1, expected 16")
                .arg(challenge_size));
            challenge_size = std::min(challenge_size, 16);
        }

        int i = 0;
        std::array<uint8_t,38> from {};
        std::copy(challenge.cbegin(), challenge.cbegin() + challenge_size,
                  from.data());
        i += challenge_size;
        if (m_socket->localAddress().protocol() ==
            QAbstractSocket::IPv4Protocol)
        {
            uint32_t ip = m_socket->localAddress().toIPv4Address();
            ip = qToBigEndian(ip);
            memcpy(&from[i], &ip, 4);
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
                memcpy(&from[i], &ip[12], 4);
                i += 4;
            }
            else
            {
                memcpy(&from[i], &ip, 16);
                i += 16;
            }
        }
        if (m_hardwareId.size() == AIRPLAY_HARDWARE_ID_SIZE)
        {
            memcpy(&from[i], m_hardwareId.constData(), AIRPLAY_HARDWARE_ID_SIZE);
            i += AIRPLAY_HARDWARE_ID_SIZE;
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                QString("Hardware MAC address size %1, expected %2")
                .arg(m_hardwareId.size()).arg(AIRPLAY_HARDWARE_ID_SIZE));
        }

        int pad = 32 - i;
        if (pad > 0)
        {
            memset(&from[i], 0, pad);
            i += pad;
        }

        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Full base64 response: '%1' size %2")
            .arg(QByteArray((char *)from.data(), i).toBase64().constData())
            .arg(i));

        auto *pctx = EVP_PKEY_CTX_new(g_devPrivKey, nullptr);
        if (nullptr == pctx)
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                QString("Cannot create ENV_PKEY_CTX from key. (%1)")
                .arg(ERR_error_string(ERR_get_error(), nullptr)));
        }
        else if (EVP_PKEY_sign_init(pctx) <= 0)
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                QString("EVP_PKEY_sign_init failed. (%1)")
                .arg(ERR_error_string(ERR_get_error(), nullptr)));
        }
        else if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) <= 0)
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                QString("Cannot set RSA_PKCS1_PADDING on context. (%1)")
                .arg(ERR_error_string(ERR_get_error(), nullptr)));
        }
        else if (EVP_PKEY_sign(pctx, to.data(), &tosize,
                          from.data(), i) <= 0)
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                QString("EVP_PKEY_sign failed. (%1)")
                .arg(ERR_error_string(ERR_get_error(), nullptr)));
        }

        QByteArray base64 = QByteArray((const char *)to.data(), tosize).toBase64();

        for (int pos = base64.size() - 1; pos > 0; pos--)
        {
            if (base64[pos] == '=')
                base64[pos] = ' ';
            else
                break;
        }
        LOG(VB_PLAYBACK, LOG_DEBUG, QString("tSize=%1 tLen=%2 tResponse=%3")
            .arg(tosize).arg(base64.size()).arg(base64.constData()));
        *m_textStream << base64.trimmed() << "\r\n";
    }

    QString responseData;
    if (option == "OPTIONS")
    {
        *m_textStream << "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, "
            "TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER, POST, GET\r\n";
    }
    else if (option == "ANNOUNCE")
    {
        QStringList lines = splitLines(content);
        for (const QString& line : std::as_const(lines))
        {
            if (line.startsWith("a=rsaaeskey:"))
            {
                QString key = line.mid(12).trimmed();
                QByteArray decodedkey = QByteArray::fromBase64(key.toLatin1());
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                    QString("RSAAESKey: %1 (base64 decoded size %2)")
                    .arg(key).arg(decodedkey.size()));

                if (LoadKey())
                {
                    size_t size = EVP_PKEY_get_size(g_devPrivKey);
                    auto *pctx = EVP_PKEY_CTX_new(g_devPrivKey /* EVP_PKEY *pkey */,
                                                  nullptr      /* ENGINE *e */);
                    m_sessionKey.resize(size);
                    size_t size_out {size};
                    if (nullptr == pctx)
                    {
                        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                            QString("Cannot create ENV_PKEY_CTX from key. (%1)")
                            .arg(ERR_error_string(ERR_get_error(), nullptr)));
                    }
                    else if (EVP_PKEY_decrypt_init(pctx) <= 0)
                    {
                        LOG(VB_PLAYBACK, LOG_WARNING, LOC + QString("EVP_PKEY_decrypt_init failed. (%1)")
                            .arg(ERR_error_string(ERR_get_error(), nullptr)));
                    }
                    else if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_OAEP_PADDING) <= 0)
                    {
                        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                            QString("Cannot set RSA_PKCS1_OAEP_PADDING on context. (%1)")
                            .arg(ERR_error_string(ERR_get_error(), nullptr)));
                    }
                    else if (EVP_PKEY_decrypt(pctx, m_sessionKey.data(), &size_out,
                                              (const unsigned char *)decodedkey.constData(), decodedkey.size()) > 0)
                    {
                        // SUCCESS
                        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                            "Successfully decrypted AES key from RSA.");
                    }
                    else
                    {
                        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                            QString("Failed to decrypt AES key from RSA. (%1)")
                            .arg(ERR_error_string(ERR_get_error(), nullptr)));
                    }
                }
            }
            else if (line.startsWith("a=aesiv:"))
            {
                QString aesiv = line.mid(8).trimmed();
                m_aesIV = QByteArray::fromBase64(aesiv.toLatin1());
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                    QString("AESIV: %1 (base64 decoded size %2)")
                    .arg(aesiv).arg(m_aesIV.size()));
            }
            else if (line.startsWith("a=fmtp:"))
            {
                m_audioFormat.clear();
                QString format = line.mid(7).trimmed();
                QList<QString> formats = format.split(' ');
                for (const QString& fmt : std::as_const(formats))
                    m_audioFormat.append(fmt.toInt());

                for (int fmt : std::as_const(m_audioFormat))
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
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
            auto *dev = qobject_cast<MythRAOPDevice*>(parent());
            if (dev != nullptr)
                dev->DeleteAllClients(this);
            gCoreContext->WantingPlayback(parent());
            m_playbackStarted = true;

            int control_port = 0;
            int timing_port = 0;
            QString data = tags["Transport"];
            QStringList items = data.split(";");
            bool events = false;

            for (const QString& item : std::as_const(items))
            {
                if (item.startsWith("control_port"))
                    control_port = item.mid(item.indexOf("=") + 1).trimmed().toUInt();
                else if (item.startsWith("timing_port"))
                    timing_port  = item.mid(item.indexOf("=") + 1).trimmed().toUInt();
                else if (item.startsWith("events"))
                    events = true;
            }

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Negotiated setup with client %1 on port %2")
                    .arg(m_socket->peerAddress().toString())
                    .arg(m_socket->peerPort()));
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
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
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("Failed to bind to client control port. "
                            "Control of audio stream may fail"));
            }
            else
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Bound to client control port %1 on port %2")
                    .arg(control_port).arg(controlbind_port));
            }
            m_clientControlPort = control_port;
            connect(m_clientControlSocket,
                    &ServerPool::newDatagram,
                    this,
                    &MythRAOPConnection::udpDataReady);

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
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("Failed to bind to client timing port. "
                            "Timing of audio stream will be incorrect"));
            }
            else
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Bound to client timing port %1 on port %2")
                    .arg(timing_port).arg(timingbind_port));
            }
            m_clientTimingPort = timing_port;
            connect(m_clientTimingSocket,
                    &ServerPool::newDatagram,
                    this,
                    &MythRAOPConnection::udpDataReady);

            if (m_eventServer)
            {
                // Should never get here, but just in case
                for (auto *client : std::as_const(m_eventClients))
                {
                    client->disconnect();
                    client->abort();
                    delete client;
                }
                m_eventClients.clear();
                m_eventServer->disconnect();
                m_eventServer->close();
                delete m_eventServer;
                m_eventServer = nullptr;
            }

            if (events)
            {
                m_eventServer = new ServerPool(this);
                m_eventPort = m_eventServer->tryListeningPort(timing_port,
                                                              RAOP_PORT_RANGE);
                if (m_eventPort < 0)
                {
                    LOG(VB_PLAYBACK, LOG_ERR, LOC +
                        "Failed to find a port for RAOP events server.");
                }
                else
                {
                    LOG(VB_PLAYBACK, LOG_INFO, LOC +
                        QString("Listening for RAOP events on port %1").arg(m_eventPort));
                    connect(m_eventServer, &ServerPool::newConnection,
                            this, &MythRAOPConnection::newEventClient);
                }
            }

            if (OpenAudioDevice())
            {
                CreateDecoder();
                    // Calculate audio card latency
                if (m_cardLatency < 0ms)
                {
                    m_adjustedLatency = m_cardLatency = AudioCardLatency();
                    // if audio isn't started, start playing 500ms worth of silence
                    // and measure timestamp difference
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                        QString("Audio hardware latency: %1ms")
                        .arg((m_cardLatency + m_networkLatency).count()));
                }
            }

            // Recreate transport line with new ports value
            QString newdata;
            bool first = true;
            for (const QString& item : std::as_const(items))
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

            // Ask for master clock value to determine time skew and average network latency
            SendTimeRequest();
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                "No Transport details found - Ignoring");
        }
    }
    else if (option == "RECORD")
    {
        if (gotRTP)
        {
            m_nextSequence  = RTPseq;
            m_nextTimestamp = framesToMs(RTPtimestamp);
        }

        // Calculate audio card latency
        if (m_cardLatency > 0ms)
        {
            *m_textStream << QString("Audio-Latency: %1")
                .arg((m_cardLatency+m_networkLatency).count());
        }
    }
    else if (option == "FLUSH")
    {
        if (gotRTP)
        {
            m_nextSequence     = RTPseq;
            m_nextTimestamp    = framesToMs(RTPtimestamp);
            m_currentTimestamp = m_nextTimestamp - m_bufferLength;
        }
        // determine RTP timestamp of last sample played
        std::chrono::milliseconds timestamp = m_audioStarted && m_audio ?
            m_audio->GetAudiotime() : m_lastTimestamp;
        *m_textStream << "RTP-Info: rtptime=" << QString::number(MsToFrame(timestamp));
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

                LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                    QString("text/parameters: name=%1 parem=%2")
                    .arg(name, param));

                if (name == "volume" && m_allowVolumeControl && m_audio)
                {
                    float volume = (param.toFloat() + 30.0F) * 100.0F / 30.0F;
                    if (volume < 0.01F)
                        volume = 0.0F;
                    LOG(VB_PLAYBACK, LOG_INFO,
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

                    LOG(VB_PLAYBACK, LOG_INFO,
                        LOC +QString("Progress: %1/%2")
                        .arg(stringFromSeconds(current),
                             stringFromSeconds(length)));
                    SendNotification(true);
                }
            }
            else if(tags["Content-Type"] == "image/none")
            {
                m_artwork.clear();
                SendNotification(false);
            }
            else if(tags["Content-Type"].startsWith("image/"))
            {
                // Receiving image coverart
                m_artwork = content;
                SendNotification(false);
            }
            else if (tags["Content-Type"] == "application/x-dmap-tagged")
            {
                // Receiving DMAP metadata
                m_dmap = decodeDMAP(content);
                LOG(VB_PLAYBACK, LOG_INFO,
                    QString("Receiving Title:%1 Artist:%2 Album:%3 Format:%4")
                    .arg(m_dmap["minm"], m_dmap["asar"],
                         m_dmap["asal"], m_dmap["asfm"]));
                SendNotification(false);
            }
        }
    }
    else if (option == "GET_PARAMETER")
    {
        if (tags.contains("Content-Type"))
        {
            if (tags["Content-Type"] == "text/parameters")
            {
                QStringList lines = splitLines(content);
                *m_textStream << "Content-Type: text/parameters\r\n";
                for (const QString& line : std::as_const(lines))
                {
                    if (line == "volume")
                    {
                        int volume = (m_allowVolumeControl && m_audio) ?
                            m_audio->GetCurrentVolume() : 0;
                        responseData += QString("volume: %1\r\n")
                            .arg((volume * 30.0F / 100.0F) - 30.0F,1,'f',6,'0');
                    }
                }
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
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Command not handled: %1")
            .arg(option));
    }
    FinishResponse(m_textStream, m_socket, option, tags["CSeq"], responseData);
}

void MythRAOPConnection::FinishAuthenticationResponse(RaopNetStream *stream,
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
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("Finished Authentication request %2, Send: %3")
        .arg(cseq).arg(socket->flush()));
}

void MythRAOPConnection::FinishResponse(RaopNetStream *stream, QTcpSocket *socket,
                                        QString &option, QString &cseq, QString &responseData)
{
    if (!stream)
        return;
    *stream << "Server: AirTunes/130.14\r\n";
    *stream << "CSeq: " << cseq << "\r\n";
    if (!responseData.isEmpty())
        *stream << "Content-Length: " << QString::number(responseData.length()) << "\r\n\r\n" << responseData;
    else
        *stream << "\r\n";
    stream->flush();
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Finished %1 %2 , Send: %3")
        .arg(option, cseq, QString::number(socket->flush())));
}

/**
 * LoadKey. Load RSA key into static variable for re-using it later
 * The RSA key is resident in memory for the entire duration of the application
 * as such RSA_free is never called on it.
 */
bool MythRAOPConnection::LoadKey(void)
{
    static QMutex s_lock;
    QMutexLocker locker(&s_lock);

    if (g_devPrivKey)
        return true;

    QString sName( "/RAOPKey.rsa" );
    FILE *file = fopen(GetConfDir().toUtf8() + sName.toUtf8(), "rb");

    if ( !file )
    {
        g_rsaLastError = tr("Failed to read key from: %1").arg(GetConfDir() + sName);
        LOG(VB_PLAYBACK, LOG_ERR, LOC + g_rsaLastError);
        return false;
    }

    g_devPrivKey = PEM_read_PrivateKey(file, nullptr, nullptr, nullptr);
    fclose(file);

    if (g_devPrivKey)
    {
        int id = EVP_PKEY_get_id(g_devPrivKey);
        int type = EVP_PKEY_type(id);
        if (type != EVP_PKEY_RSA)
        {
            g_rsaLastError = tr("Key is not a RSA private key.");
            EVP_PKEY_free(g_devPrivKey);
            g_devPrivKey = nullptr;
            LOG(VB_PLAYBACK, LOG_ERR, LOC + g_rsaLastError);
            return false;
        }
        g_rsaLastError = "";
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "Loaded RSA private key");
        return true;
    }

    g_rsaLastError = tr("Failed to load RSA private key.");
    LOG(VB_PLAYBACK, LOG_ERR, LOC + g_rsaLastError);
    return false;
}

RawHash MythRAOPConnection::FindTags(const QStringList &lines)
{
    RawHash result;
    if (lines.isEmpty())
        return result;

    for (const QString& line : std::as_const(lines))
    {
        int index = line.indexOf(":");
        if (index > 0)
        {
            result.insert(line.left(index).trimmed(),
                          line.mid(index + 1).trimmed());
        }
    }
    return result;
}

QStringList MythRAOPConnection::splitLines(const QByteArray &lines)
{
    QStringList list;
    QTextStream stream(lines);

    QString line = stream.readLine();
    while (!line.isEmpty())
    {
        list.append(line);
        line = stream.readLine();
    }

    return list;
}

/**
 * stringFromSeconds:
 *
 * Usage: stringFromSeconds(timeInSeconds)
 * Description: create a string in the format HH:mm:ss from a duration in seconds
 * HH: will not be displayed if there's less than one hour
 */
QString MythRAOPConnection::stringFromSeconds(int timeInSeconds)
{
    QTime time = QTime(0,0).addSecs(timeInSeconds);
    return time.toString(time.hour() > 0 ? "HH:mm:ss" : "mm:ss");
}

/**
 * framesDuration
 * Description: return the duration in ms of frames
 *
 */
std::chrono::milliseconds MythRAOPConnection::framesToMs(uint64_t frames) const
{
    return std::chrono::milliseconds((frames * 1000ULL) / m_frameRate);
}

uint64_t MythRAOPConnection::MsToFrame(std::chrono::milliseconds millis) const
{
    return millis.count() * m_frameRate / 1000ULL;
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
    m_codec = avcodec_find_decoder(AV_CODEC_ID_ALAC);
    if (!m_codec)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC
            + "Failed to create ALAC decoder- going silent...");
        return false;
    }

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (m_codecContext)
    {
        auto *extradata = new unsigned char[36];
        memset(extradata, 0, 36);
        if (m_audioFormat.size() < 12)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
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
        m_codecContext->extradata = extradata;
        m_codecContext->extradata_size = 36;
        m_codecContext->ch_layout.nb_channels = m_channels;
        if (avcodec_open2(m_codecContext, m_codec, nullptr) < 0)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                "Failed to open ALAC decoder - going silent...");
            DestroyDecoder();
            return false;
        }
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "Opened ALAC decoder.");
        m_codecContext->sample_rate = m_audioFormat[11]; // sampleRate
    }

    return true;
}

void MythRAOPConnection::DestroyDecoder(void)
{
    if (m_codecContext)
    {
        avcodec_free_context(&m_codecContext);
    }
    m_codec = nullptr;
}

bool MythRAOPConnection::OpenAudioDevice(void)
{
    CloseAudioDevice();

    QString passthru = gCoreContext->GetBoolSetting("PassThruDeviceOverride", false)
                        ? gCoreContext->GetSetting("PassThruOutputDevice") : QString();
    QString device = gCoreContext->GetSetting("AudioOutputDevice");

    m_audio = AudioOutput::OpenAudio(device, passthru, FORMAT_S16, m_channels,
                                     AV_CODEC_ID_NONE, m_frameRate, AUDIOOUTPUT_MUSIC,
                                     m_allowVolumeControl, false);
    if (!m_audio)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "Failed to open audio device. Going silent...");
        CloseAudioDevice();
        StartAudioTimer();
        return false;
    }

    QString error = m_audio->GetError();
    if (!error.isEmpty())
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("Audio not initialised. Message was '%1'")
            .arg(error));
        CloseAudioDevice();
        StartAudioTimer();
        return false;
    }

    StopAudioTimer();
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "Opened audio device.");
    return true;
}

void MythRAOPConnection::CloseAudioDevice(void)
{
    delete m_audio;
    m_audio = nullptr;
}

void MythRAOPConnection::StartAudioTimer(void)
{
    if (m_audioTimer)
        return;

    m_audioTimer = new QTimer();
    connect(m_audioTimer, &QTimer::timeout, this, &MythRAOPConnection::audioRetry);
    m_audioTimer->start(5s);
}

void MythRAOPConnection::StopAudioTimer(void)
{
    if (m_audioTimer)
    {
        m_audioTimer->stop();
    }
    delete m_audioTimer;
    m_audioTimer = nullptr;
}

/**
 * AudioCardLatency:
 * Description: Play silence and calculate audio latency between input / output
 */
std::chrono::milliseconds MythRAOPConnection::AudioCardLatency(void)
{
    if (!m_audio)
        return 0ms;

    auto *samples = (int16_t *)av_mallocz(AudioOutput::kMaxSizeBuffer);
    int frames = AUDIOCARD_BUFFER.count() * m_frameRate / 1000;
    m_audio->AddData((char *)samples,
                     frames * (m_sampleSize>>3) * m_channels,
                     0ms,
                     frames);
    av_free(samples);
    usleep(duration_cast<std::chrono::microseconds>(AUDIOCARD_BUFFER).count());
    std::chrono::milliseconds audiots = m_audio->GetAudiotime();
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("AudioCardLatency: ts=%1ms")
        .arg(audiots.count()));
    return AUDIOCARD_BUFFER - audiots;
}

void MythRAOPConnection::newEventClient(QTcpSocket *client)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("New connection from %1:%2 for RAOP events server.")
        .arg(client->peerAddress().toString()).arg(client->peerPort()));

    m_eventClients.append(client);
    connect(client, &QAbstractSocket::disconnected, this, &MythRAOPConnection::deleteEventClient);
}

void MythRAOPConnection::deleteEventClient(void)
{
    auto *client = qobject_cast<QTcpSocket *>(sender());
    QString label;

    if (client != nullptr)
        label = QString("%1:%2").arg(client->peerAddress().toString()).arg(client->peerPort());
    else
        label = QString("unknown");

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("%1 disconnected from RAOP events server.").arg(label));
}

void MythRAOPConnection::SendNotification(bool update)
{
    QImage image = m_artwork.isEmpty() ? QImage() : QImage::fromData(m_artwork);
    auto duration = std::chrono::seconds(
        lroundf(static_cast<float>(m_progressEnd-m_progressStart) / m_frameRate));
    int position =
        (m_progressCurrent-m_progressStart) / m_frameRate;

    MythNotification *n = nullptr;

    if (!update || !m_firstSend)
    {
        n = new MythMediaNotification(MythNotification::kNew,
                                      image, m_dmap, duration, position);
    }
    else
    {
        n = new MythPlaybackNotification(MythNotification::kUpdate,
                                         duration, position);
    }
    n->SetId(m_id);
    n->SetParent(this);
    n->SetDuration(5s);
    n->SetFullScreen(gCoreContext->GetBoolSetting("AirPlayFullScreen"));
    GetNotificationCenter()->Queue(*n);
    m_firstSend = true;
    delete n;
}
