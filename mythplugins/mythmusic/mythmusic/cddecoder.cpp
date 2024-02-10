#define DO_NOT_WANT_PARANOIA_COMPATIBILITY
#include "cddecoder.h"

// C
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// Qt
#include <QFile>
#include <QIODevice>
#include <QObject>
#include <QString>

// libcdio
// cdda already included via cddecoder.h
#include <cdio/logging.h>

// MythTV
#include <libmyth/audio/audiooutput.h>
#include <libmyth/mythcontext.h>
#include <libmythmetadata/musicmetadata.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

// MythMusic
#include "constants.h"
static constexpr const char* CDEXT { ".cda" };
static constexpr long kSamplesPerSec { 44100 };

// Handle cdio log output
static void logger(cdio_log_level_t level, const char *message)
{
    switch (level)
    {
    case CDIO_LOG_DEBUG:
        break;
    case CDIO_LOG_INFO:
        LOG(VB_MEDIA, LOG_DEBUG, QString("INFO cdio: %1").arg(message));
        break;
    case CDIO_LOG_WARN:
        LOG(VB_MEDIA, LOG_DEBUG, QString("WARN cdio: %1").arg(message));
        break;
    case CDIO_LOG_ERROR:
    case CDIO_LOG_ASSERT:
        LOG(VB_GENERAL, LOG_ERR, QString("ERROR cdio: %1").arg(message));
        break;
    }
}

// Open a cdio device
static CdIo_t * openCdio(const QString& name)
{
    // Setup log handler
    static int s_logging;
    if (!s_logging)
    {
        s_logging = 1;
        cdio_log_set_handler(&logger);
    }

    CdIo_t *cdio = cdio_open(name.toLatin1(), DRIVER_DEVICE);
    if (!cdio)
    {
        LOG(VB_MEDIA, LOG_INFO, QString("CdDecoder: cdio_open(%1) failed").
            arg(name));
    }
    return cdio;
}

// Stack-based cdio device open
class StCdioDevice
{
    CdIo_t* m_cdio {nullptr};

    void* operator new(std::size_t); // Stack only
    // No copying
    StCdioDevice(const StCdioDevice&);
    StCdioDevice& operator =(const StCdioDevice&);

public:
    explicit StCdioDevice(const QString& dev) : m_cdio(openCdio(dev)) { }
    ~StCdioDevice() { if (m_cdio) cdio_destroy(m_cdio); }

    operator CdIo_t*() const { return m_cdio; } // NOLINT(google-explicit-constructor)
};


CdDecoder::CdDecoder(const QString &file, DecoderFactory *d, AudioOutput *o) :
    Decoder(d, o)
{
    setURL(file);
}

// virtual
CdDecoder::~CdDecoder()
{
    if (m_inited)
        deinit();
}

void CdDecoder::setDevice(const QString &dev)
{
    m_deviceName = dev;
#ifdef WIN32
    // libcdio needs the drive letter with no path
    if (m_deviceName.endsWith('\\'))
        m_deviceName.chop(1);
#endif
}

// pure virtual
void CdDecoder::stop()
{
    m_userStop = true;
}

// private
void CdDecoder::writeBlock()
{
    while (m_seekTime <= +0.)
    {
        if(output()->AddFrames(m_outputBuf, m_bksFrames, -1ms))
        {
            if (m_outputAt >= m_bks)
            {
                m_outputAt -= m_bks;
                std::memmove(m_outputBuf, m_outputBuf + m_bks, m_outputAt);
            }
            break;
        }
        ::usleep(output()->GetAudioBufferedTime().count()<<9);
    }
}

//static
QRecursiveMutex& CdDecoder::getCdioMutex()
{
    static QRecursiveMutex s_mtx;
    return s_mtx;
}

// pure virtual
bool CdDecoder::initialize()
{
    if (m_inited)
        return true;

    m_inited = m_userStop = m_finish = false;
    m_freq = m_bitrate = 0L;
    m_stat = DecoderEvent::kError;
    m_chan = 0;
    m_seekTime = -1.;

    if (output())
        output()->PauseUntilBuffered();

    m_trackNum = getURL().section('.', 0, 0).toUInt();

    QMutexLocker lock(&getCdioMutex());

    m_cdio = openCdio(m_deviceName);
    if (!m_cdio)
        return false;

    m_start = cdio_get_track_lsn(m_cdio, m_trackNum);
    m_end = cdio_get_track_last_lsn(m_cdio, m_trackNum);
    if (CDIO_INVALID_LSN  == m_start ||
        CDIO_INVALID_LSN  == m_end)
    {
        LOG(VB_MEDIA, LOG_INFO, "CdDecoder: No tracks on " + m_deviceName);
        cdio_destroy(m_cdio), m_cdio = nullptr;
        return false;
    }

    LOG(VB_MEDIA, LOG_DEBUG, QString("CdDecoder track=%1 lsn start=%2 end=%3")
            .arg(m_trackNum).arg(m_start).arg(m_end));
    m_curPos = m_start;

    m_device = cdio_cddap_identify_cdio(m_cdio, 0, nullptr);
    if (nullptr == m_device)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error: CdDecoder: cdio_cddap_identify(%1) failed")
                .arg(m_deviceName));
        cdio_destroy(m_cdio), m_cdio = nullptr;
        return false;
    }

    cdio_cddap_verbose_set(m_device,
        VERBOSE_LEVEL_CHECK(VB_MEDIA, LOG_ANY) ? CDDA_MESSAGE_PRINTIT :
            CDDA_MESSAGE_FORGETIT,
        VERBOSE_LEVEL_CHECK(VB_MEDIA, LOG_DEBUG) ? CDDA_MESSAGE_PRINTIT :
            CDDA_MESSAGE_FORGETIT);

    if (DRIVER_OP_SUCCESS == cdio_cddap_open(m_device))
    {
        // cdio_get_track_last_lsn is unreliable on discs with data at end
        lsn_t end2 = cdio_cddap_track_lastsector(m_device, m_trackNum);
        if (end2 < m_end)
        {
            LOG(VB_MEDIA, LOG_INFO, QString("CdDecoder: trim last lsn from %1 to %2")
                .arg(m_end).arg(end2));
            m_end = end2;
        }

        // FIXME can't use cdio_paranoia until we find a way to cleanly
        // detect when the user has ejected a CD otherwise we enter a
        // recursive loop in cdio_paranoia_read_limited()
        //m_paranoia = cdio_paranoia_init(m_device);
        if (nullptr != m_paranoia)
        {
            cdio_paranoia_modeset(m_paranoia, PARANOIA_MODE_DISABLE);
            (void)cdio_paranoia_seek(m_paranoia, m_start, SEEK_SET);
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, "CD reading with paranoia is disabled");
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Warn: drive '%1' is not cdda capable").
            arg(m_deviceName));
    }

    int chnls = cdio_get_track_channels(m_cdio, m_trackNum);
    m_chan = chnls > 0 ? chnls : 2;
    m_freq = kSamplesPerSec;

    if (output())
    {
        const AudioSettings settings(FORMAT_S16, m_chan,
            AV_CODEC_ID_PCM_S16LE, m_freq, false /* AC3/DTS passthru */);
        output()->Reconfigure(settings);
        output()->SetSourceBitrate(m_freq * m_chan * 16);
    }

    // 20ms worth
    m_bks = (m_freq * m_chan * 2) / 50;
    m_bksFrames = m_freq / 50;
    // decode 8 bks worth of samples each time we need more
    m_decodeBytes = m_bks << 3;

    m_outputBuf = reinterpret_cast< char* >(
        ::av_malloc(m_decodeBytes + CDIO_CD_FRAMESIZE_RAW * 2));
    m_outputAt = 0;

    setCDSpeed(2);

    m_inited = true;

    return m_inited;
}

// pure virtual
void CdDecoder::seek(double pos)
{
    m_seekTime = pos;
    if (output())
        output()->PauseUntilBuffered();
}

// private
void CdDecoder::deinit()
{
    setCDSpeed(-1);

    QMutexLocker lock(&getCdioMutex());

    if (m_paranoia)
        cdio_paranoia_free(m_paranoia), m_paranoia = nullptr;
    if (m_device)
        cdio_cddap_close(m_device), m_device = nullptr, m_cdio = nullptr;
    if (m_cdio)
        cdio_destroy(m_cdio), m_cdio = nullptr;

    if (m_outputBuf)
        ::av_free(m_outputBuf), m_outputBuf = nullptr;

    m_inited = m_userStop = m_finish = false;
    m_freq = m_bitrate = 0L;
    m_stat = DecoderEvent::kFinished;
    m_chan = 0;
    setOutput(nullptr);
}

// private virtual
void CdDecoder::run()
{
    RunProlog();

    if (!m_inited)
    {
        RunEpilog();
        return;
    }

    m_stat = DecoderEvent::kDecoding;
    // NB block scope required to prevent re-entrancy
    {
        DecoderEvent e(m_stat);
        dispatch(e);
    }

    // account for possible frame expansion in aobase (upmix, float conv)
    const std::size_t thresh = m_bks * 6;

    while (!m_finish && !m_userStop)
    {
        if (m_seekTime >= +0.)
        {
            m_curPos = m_start + static_cast< lsn_t >(
                (m_seekTime * kSamplesPerSec) / CD_FRAMESAMPLES);
            if (m_paranoia)
            {
                QMutexLocker lock(&getCdioMutex());
                cdio_paranoia_seek(m_paranoia, m_curPos, SEEK_SET);
            }

            m_outputAt = 0;
            m_seekTime = -1.;
        }

        if (m_outputAt < m_bks)
        {
            while (m_outputAt < m_decodeBytes &&
                   !m_finish && !m_userStop && m_seekTime <= +0.)
            {
                if (m_curPos < m_end)
                {
                    QMutexLocker lock(&getCdioMutex());
                    if (m_paranoia)
                    {
                        int16_t *cdbuffer = cdio_paranoia_read_limited(
                                                m_paranoia, nullptr, 10);
                        if (cdbuffer)
                            memcpy(&m_outputBuf[m_outputAt],
                                cdbuffer, CDIO_CD_FRAMESIZE_RAW);
                    }
                    else
                    {
                        driver_return_code_t c = cdio_read_audio_sector(
                            m_cdio, &m_outputBuf[m_outputAt], m_curPos);
                        if (DRIVER_OP_SUCCESS != c)
                        {
                            LOG(VB_MEDIA, LOG_DEBUG,
                                QString("cdio_read_audio_sector(%1) error %2").
                                arg(m_curPos).arg(c));
                            memset( &m_outputBuf[m_outputAt],
                                0, CDIO_CD_FRAMESIZE_RAW);

                            // stop if we got an error
                            m_userStop = true;
                        }
                    }

                    m_outputAt += CDIO_CD_FRAMESIZE_RAW;
                    ++(m_curPos);
                }
                else
                {
                    m_finish = true;
                }
            }
        }

        if (!output())
            continue;

        // Wait until we need to decode or supply more samples
        uint fill = 0;
        uint total = 0;
        while (!m_finish && !m_userStop && m_seekTime <= +0.)
        {
            output()->GetBufferStatus(fill, total);
            // Make sure we have decoded samples ready and that the
            // audiobuffer is reasonably populated
            if (fill < (thresh << 6))
                break;
            // Wait for half of the buffer to drain
            ::usleep(output()->GetAudioBufferedTime().count()<<9);
        }

        // write a block if there's sufficient space for it
        if (!m_userStop &&
            m_outputAt >= m_bks &&
            fill <= total - thresh)
        {
            writeBlock();
        }
    }

    if (m_userStop)
        m_inited = false;
    else if (output())
    {
        // Drain our buffer
        while (m_outputAt >= m_bks)
            writeBlock();

        // Drain ao buffer
        output()->Drain();
    }

    if (m_finish)
        m_stat = DecoderEvent::kFinished;
    else if (m_userStop)
        m_stat = DecoderEvent::kStopped;
    else
        m_stat = DecoderEvent::kError;

    // NB block scope required to step onto next track
    {
        DecoderEvent e(m_stat);
        dispatch(e);
    }

    deinit();

    RunEpilog();
}

//public
void CdDecoder::setCDSpeed(int speed)
{
    QMutexLocker lock(&getCdioMutex());

    StCdioDevice cdio(m_deviceName);
    if (cdio)
    {
        driver_return_code_t c = cdio_set_speed(cdio, speed >= 0 ? speed : 1);
        if (DRIVER_OP_SUCCESS != c)
        {
            LOG(VB_MEDIA, LOG_INFO,
                QString("Error: cdio_set_speed('%1',%2) failed").
                arg(m_deviceName).arg(speed));
        }
    }
}

//public
int CdDecoder::getNumTracks()
{
    QMutexLocker lock(&getCdioMutex());

    StCdioDevice cdio(m_deviceName);
    if (!cdio)
        return 0;

    track_t tracks = cdio_get_num_tracks(cdio);
    if (CDIO_INVALID_TRACK != tracks)
        LOG(VB_MEDIA, LOG_DEBUG, QString("getNumTracks = %1").arg(tracks));
    else
        tracks = -1;

    return tracks;
}

//public
int CdDecoder::getNumCDAudioTracks()
{
    QMutexLocker lock(&getCdioMutex());

    StCdioDevice cdio(m_deviceName);
    if (!cdio)
        return 0;

    int nAudio = 0;
    const track_t last = cdio_get_last_track_num(cdio);
    if (CDIO_INVALID_TRACK != last)
    {
        for (track_t t = cdio_get_first_track_num(cdio) ; t <= last; ++t)
        {
            if (TRACK_FORMAT_AUDIO == cdio_get_track_format(cdio, t))
                ++nAudio;
        }
        LOG(VB_MEDIA, LOG_DEBUG, QString("getNumCDAudioTracks = %1").arg(nAudio));
    }

    return nAudio;
}

//public
MusicMetadata* CdDecoder::getMetadata(int track)
{
    m_setTrackNum = track;
    return getMetadata();
}


// Create a TOC
static lsn_t s_lastAudioLsn;

//virtual
MusicMetadata *CdDecoder::getMetadata()
{
    QString artist;
    QString album;
    QString compilation_artist;
    QString title;
    QString genre;
    int year = 0;
    std::chrono::milliseconds length = 0s;
    track_t tracknum = 0;

    if (-1 == m_setTrackNum)
        tracknum = getURL().toUInt();
    else
    {
        tracknum = m_setTrackNum;
        setURL(QString("%1%2").arg(tracknum).arg(CDEXT));
    }

    QMutexLocker lock(&getCdioMutex());

    StCdioDevice cdio(m_deviceName);
    if (!cdio)
        return nullptr;

    const track_t lastTrack = cdio_get_last_track_num(cdio);
    if (CDIO_INVALID_TRACK == lastTrack)
        return nullptr;

    if (TRACK_FORMAT_AUDIO != cdio_get_track_format(cdio, tracknum))
        return nullptr;

    // Assume disc changed if max LSN different
    bool isDiscChanged = false;
    static lsn_t s_totalSectors;
    lsn_t totalSectors = cdio_get_track_lsn(cdio, CDIO_CDROM_LEADOUT_TRACK);
    if (s_totalSectors != totalSectors)
    {
        s_totalSectors = totalSectors;
        isDiscChanged = true;
    }

    // NB cdio_get_track_last_lsn is unreliable for the last audio track
    // of discs with data tracks beyond
    lsn_t end = cdio_get_track_last_lsn(cdio, tracknum);
    if (isDiscChanged)
    {
        const track_t audioTracks = getNumCDAudioTracks();
        s_lastAudioLsn = cdio_get_track_last_lsn(cdio, audioTracks);

        if (audioTracks < lastTrack)
        {
            cdrom_drive_t *dev = cdio_cddap_identify_cdio(cdio, 0, nullptr);
            if (nullptr != dev)
            {
                if (DRIVER_OP_SUCCESS == cdio_cddap_open(dev))
                {
                    // NB this can be S L O W  but is reliable
                    lsn_t end2 = cdio_cddap_track_lastsector(dev,
                        getNumCDAudioTracks());
                    if (CDIO_INVALID_LSN != end2)
                        s_lastAudioLsn = end2;
                }
                cdio_cddap_close_no_free_cdio(dev);
            }
        }
    }

    if (s_lastAudioLsn && s_lastAudioLsn < end)
        end = s_lastAudioLsn;

    const lsn_t start = cdio_get_track_lsn(cdio, tracknum);
    if (CDIO_INVALID_LSN != start && CDIO_INVALID_LSN != end)
    {
        length = std::chrono::milliseconds(((end - start + 1) * 1000 + CDIO_CD_FRAMES_PER_SEC/2) /
                                           CDIO_CD_FRAMES_PER_SEC);
    }

    bool isCompilation = false;

// Disabled - cd-text access on discs without it is S L O W
#define CDTEXT 0 // NOLINT(cppcoreguidelines-macro-usage)
#if CDTEXT
    static int s_iCdtext;
    if (isDiscChanged)
        s_iCdtext = -1;

    if (s_iCdtext)
    {
        // cdio_get_cdtext can't take >5 seconds on some CD's without cdtext
        if (s_iCdtext < 0)
            LOG(VB_MEDIA, LOG_INFO,
                QString("Getting cdtext for track %1...").arg(tracknum));
        cdtext_t * cdtext = cdio_get_cdtext(m_cdio, tracknum);
        if (nullptr != cdtext)
        {
            genre = cdtext_get_const(CDTEXT_GENRE, cdtext);
            artist = cdtext_get_const(CDTEXT_PERFORMER, cdtext);
            title = cdtext_get_const(CDTEXT_TITLE, cdtext);
            const char* isrc = cdtext_get_const(CDTEXT_ISRC, cdtext);
            /* ISRC codes are 12 characters long, in the form CCXXXYYNNNNN
             * CC = country code
             * XXX = registrant e.g. BMG
             * CC = year withou century
             * NNNNN = unique ID
             */
            if (isrc && strlen(isrc) >= 7)
            {
                year = (isrc[5] - '0') * 10 + (isrc[6] - '0');
                year += (year <= 30) ? 2000 : 1900;
            }

            cdtext_destroy(cdtext);

            if (!title.isNull())
            {
                if (s_iCdtext < 0)
                    LOG(VB_MEDIA, LOG_INFO, "Found cdtext track title");
                s_iCdtext = 1;

                // Get disc info
                cdtext = cdio_get_cdtext(cdio, 0);
                if (nullptr != cdtext)
                {
                    compilation_artist = cdtext_get_const(
                        CDTEXT_PERFORMER, cdtext);
                    if (!compilation_artist.isEmpty() &&
                            artist != compilation_artist)
                        isCompilation = true;

                    album = cdtext_get_const(CDTEXT_TITLE, cdtext);

                    if (genre.isNull())
                        genre = cdtext_get_const(CDTEXT_GENRE, cdtext);

                    cdtext_destroy(cdtext);
                }
            }
            else
            {
                if (s_iCdtext < 0)
                    LOG(VB_MEDIA, LOG_INFO, "No cdtext title for track");
                s_iCdtext = 0;
            }
        }
        else
        {
            if (s_iCdtext < 0)
                LOG(VB_MEDIA, LOG_INFO, "No cdtext");
            s_iCdtext = 0;
        }
    }

    if (title.isEmpty() || artist.isEmpty() || album.isEmpty())
#endif // CDTEXT
    {
#ifdef HAVE_MUSICBRAINZ
        if (isDiscChanged)
        {
            // lazy load whole CD metadata
            getMusicBrainz().queryForDevice(m_deviceName);
        }
        if (getMusicBrainz().hasMetadata(m_setTrackNum))
        {
            auto *metadata = getMusicBrainz().getMetadata(m_setTrackNum);
            if (metadata)
            {
                metadata->setFilename(getURL());
                return metadata;
            }
        }
#endif // HAVE_MUSICBRAINZ
    }

    if (compilation_artist.toLower().left(7) == "various")
        compilation_artist = tr("Various Artists");

    if (artist.isEmpty())
    {
        artist = compilation_artist;
        compilation_artist.clear();
    }

    if (title.isEmpty())
        title = tr("Track %1").arg(tracknum);

    auto *m = new MusicMetadata(getURL(), artist, compilation_artist, album,
                                title, genre, year, tracknum, length);
    if (m)
        m->setCompilation(isCompilation);

    return m;
}

#ifdef HAVE_MUSICBRAINZ

MusicBrainz & CdDecoder::getMusicBrainz()
{
    static MusicBrainz s_musicBrainz;
    return s_musicBrainz;
}

#endif // HAVE_MUSICBRAINZ


// pure virtual
bool CdDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).toLower() == extension());
}

// pure virtual
const QString &CdDecoderFactory::extension() const
{
    static QString s_ext(CDEXT);
    return s_ext;
}

// pure virtual
const QString &CdDecoderFactory::description() const
{
    static QString s_desc(tr("Audio CD parser"));
    return s_desc;
}

// pure virtual
Decoder *CdDecoderFactory::create(const QString &file, AudioOutput *output, bool deletable)
{
   if (deletable)
        return new CdDecoder(file, this, output);

    static CdDecoder *s_decoder;
    if (! s_decoder)
    {
        s_decoder = new CdDecoder(file, this, output);
    }
    else
    {
        s_decoder->setURL(file);
        s_decoder->setOutput(output);
    }

    return s_decoder;
}

