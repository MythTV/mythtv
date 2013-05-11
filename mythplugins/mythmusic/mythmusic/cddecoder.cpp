#define DO_NOT_WANT_PARANOIA_COMPATIBILITY
#include "cddecoder.h"

// C
#include <cstdlib>
#include <cstring>

#include <unistd.h>

// Qt
#include <QIODevice>
#include <QFile>
#include <QObject>
#include <QString>

// libcdio
#include <cdio/cdda.h>
#include <cdio/logging.h>

// MythTV
#include <audiooutput.h>
#include <mythcontext.h>
#include <musicmetadata.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

// MythMusic
#include "constants.h"
#include "cddb.h"

#define CDEXT ".cda"
const unsigned kSamplesPerSec = 44100;

// Handle cdio log output
static void logger(cdio_log_level_t level, const char message[])
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

    CdIo_t *cdio = cdio_open(name.toAscii(), DRIVER_DEVICE);
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
    CdIo_t* m_cdio;

    void* operator new(std::size_t); // Stack only
    // No copying
    StCdioDevice(const StCdioDevice&);
    StCdioDevice& operator =(const StCdioDevice&);

public:
    StCdioDevice(const QString& dev) : m_cdio(openCdio(dev)) { }
    ~StCdioDevice() { if (m_cdio) cdio_destroy(m_cdio); }

    operator CdIo_t*() const { return m_cdio; }
};


CdDecoder::CdDecoder(const QString &file, DecoderFactory *d, AudioOutput *o) :
    Decoder(d, o),
    m_inited(false),   m_user_stop(false),
    m_devicename(""),
    m_stat(DecoderEvent::Error),
    m_output_buf(NULL),
    m_output_at(0),    m_bks(0),
    m_bksFrames(0),    m_decodeBytes(0),
    m_finish(false),
    m_freq(0),         m_bitrate(0),
    m_chan(0),
    m_seekTime(-1.),
    m_settracknum(-1), m_tracknum(0),
    m_cdio(0),        m_device(0), m_paranoia( 0),
    m_start(CDIO_INVALID_LSN),
    m_end(CDIO_INVALID_LSN),
    m_curpos(CDIO_INVALID_LSN)
{
    setFilename(file);
}

// virtual
CdDecoder::~CdDecoder()
{
    if (m_inited)
        deinit();
}

void CdDecoder::setDevice(const QString &dev)
{
    m_devicename = dev;
#ifdef WIN32
    // libcdio needs the drive letter with no path
    if (m_devicename.endsWith('\\'))
        m_devicename.chop(1);
#endif
}

// pure virtual
void CdDecoder::stop()
{
    m_user_stop = true;
}

// private
void CdDecoder::writeBlock()
{
    while (m_seekTime <= +0.)
    {
        if(output()->AddFrames(m_output_buf, m_bksFrames, -1))
        {
            if (m_output_at >= m_bks)
            {
                m_output_at -= m_bks;
                std::memmove(m_output_buf, m_output_buf + m_bks,
                    m_output_at);
            }
            break;
        }
        else
            ::usleep(output()->GetAudioBufferedTime()<<9);
    }
}

//static
QMutex& CdDecoder::getCdioMutex()
{
    static QMutex mtx(QMutex::Recursive);
    return mtx;
}

// pure virtual
bool CdDecoder::initialize()
{
    if (m_inited)
        return true;

    m_inited = m_user_stop = m_finish = false;
    m_freq = m_bitrate = 0L;
    m_stat = DecoderEvent::Error;
    m_chan = 0;
    m_seekTime = -1.;

    if (output())
        output()->PauseUntilBuffered();

    QFile* file = dynamic_cast< QFile* >(input()); // From QIODevice*
    if (file)
    {
        setFilename(file->fileName());
        m_tracknum = getFilename().section('.', 0, 0).toUInt();
    }

    QMutexLocker lock(&getCdioMutex());

    m_cdio = openCdio(m_devicename);
    if (!m_cdio)
        return false;

    m_start = cdio_get_track_lsn(m_cdio, m_tracknum);
    m_end = cdio_get_track_last_lsn(m_cdio, m_tracknum);
    if (CDIO_INVALID_LSN  == m_start ||
        CDIO_INVALID_LSN  == m_end)
    {
        LOG(VB_MEDIA, LOG_INFO, "CdDecoder: No tracks on " + m_devicename);
        cdio_destroy(m_cdio), m_cdio = 0;
        return false;
    }

    LOG(VB_MEDIA, LOG_DEBUG, QString("CdDecoder track=%1 lsn start=%2 end=%3")
            .arg(m_tracknum).arg(m_start).arg(m_end));
    m_curpos = m_start;

    m_device = cdio_cddap_identify_cdio(m_cdio, 0, NULL);
    if (NULL == m_device)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error: CdDecoder: cdio_cddap_identify(%1) failed")
                .arg(m_devicename));
        cdio_destroy(m_cdio), m_cdio = 0;
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
        lsn_t end2 = cdio_cddap_track_lastsector(m_device, m_tracknum);
        if (end2 < m_end)
        {
            LOG(VB_MEDIA, LOG_INFO, QString("CdDecoder: trim last lsn from %1 to %2")
                .arg(m_end).arg(end2));
            m_end = end2;
        }

        m_paranoia = cdio_paranoia_init(m_device);
        if (NULL != m_paranoia)
        {
            cdio_paranoia_modeset(m_paranoia, PARANOIA_MODE_DISABLE);
            (void)cdio_paranoia_seek(m_paranoia, m_start, SEEK_SET);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Warn: CD reading with paranoia is disabled");
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Warn: drive '%1' is not cdda capable").
            arg(m_devicename));
    }

    int chnls = cdio_get_track_channels(m_cdio, m_tracknum);
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

    m_output_buf = reinterpret_cast< char* >(
        ::av_malloc(m_decodeBytes + CDIO_CD_FRAMESIZE_RAW * 2));
    m_output_at = 0;

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
        cdio_paranoia_free(m_paranoia), m_paranoia = 0;
    if (m_device)
        cdio_cddap_close(m_device), m_device = 0, m_cdio = 0;
    if (m_cdio)
        cdio_destroy(m_cdio), m_cdio = 0;

    if (m_output_buf)
        ::av_free(m_output_buf), m_output_buf = NULL;

    m_inited = m_user_stop = m_finish = false;
    m_freq = m_bitrate = 0L;
    m_stat = DecoderEvent::Finished;
    m_chan = 0;
    setOutput(0);
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

    m_stat = DecoderEvent::Decoding;
    // NB block scope required to prevent re-entrancy
    {
        DecoderEvent e(m_stat);
        dispatch(e);
    }

    // account for possible frame expansion in aobase (upmix, float conv)
    const std::size_t thresh = m_bks * 6;

    while (!m_finish && !m_user_stop)
    {
        if (m_seekTime >= +0.)
        {
            m_curpos = m_start + static_cast< lsn_t >(
                (m_seekTime * kSamplesPerSec) / CD_FRAMESAMPLES);
            if (m_paranoia)
            {
                QMutexLocker lock(&getCdioMutex());
                cdio_paranoia_seek(m_paranoia, m_curpos, SEEK_SET);
            }

            m_output_at = 0;
            m_seekTime = -1.;
        }

        if (m_output_at < m_bks)
        {
            while (m_output_at < m_decodeBytes &&
                   !m_finish && !m_user_stop && m_seekTime <= +0.)
            {
                if (m_curpos < m_end)
                {
                    QMutexLocker lock(&getCdioMutex());
                    if (m_paranoia)
                    {
                        int16_t *cdbuffer = cdio_paranoia_read_limited(
                                                m_paranoia, 0, 10);
                        if (cdbuffer)
                            memcpy(&m_output_buf[m_output_at],
                                cdbuffer, CDIO_CD_FRAMESIZE_RAW);
                    }
                    else
                    {
                        driver_return_code_t c = cdio_read_audio_sector(
                            m_cdio, &m_output_buf[m_output_at],
                            m_curpos);
                        if (DRIVER_OP_SUCCESS != c)
                        {
                            LOG(VB_MEDIA, LOG_DEBUG,
                                QString("cdio_read_audio_sector(%1) error %2").
                                arg(m_curpos).arg(c));
                            memset( &m_output_buf[m_output_at],
                                0, CDIO_CD_FRAMESIZE_RAW);
                        }
                    }

                    m_output_at += CDIO_CD_FRAMESIZE_RAW;
                    ++(m_curpos);
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
        uint fill = 0, total = 0;
        while (!m_finish && !m_user_stop && m_seekTime <= +0.)
        {
            output()->GetBufferStatus(fill, total);
            // Make sure we have decoded samples ready and that the
            // audiobuffer is reasonably populated
            if (fill < (thresh << 6))
                break;
            else
            {
                // Wait for half of the buffer to drain
                ::usleep(output()->GetAudioBufferedTime()<<9);
            }
        }

        // write a block if there's sufficient space for it
        if (!m_user_stop &&
            m_output_at >= m_bks &&
            fill <= total - thresh)
        {
            writeBlock();
        }
    }

    if (m_user_stop)
        m_inited = false;
    else if (output())
    {
        // Drain our buffer
        while (m_output_at >= m_bks)
            writeBlock();

        // Drain ao buffer
        output()->Drain();
    }

    if (m_finish)
        m_stat = DecoderEvent::Finished;
    else if (m_user_stop)
        m_stat = DecoderEvent::Stopped;
    else
        m_stat = DecoderEvent::Error;

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

    StCdioDevice cdio(m_devicename);
    if (cdio)
    {
        driver_return_code_t c = cdio_set_speed(cdio, speed >= 0 ? speed : 1);
        if (DRIVER_OP_SUCCESS != c)
        {
            LOG(VB_MEDIA, LOG_INFO,
                QString("Error: cdio_set_speed('%1',%2) failed").
                arg(m_devicename).arg(speed));
        }
    }
}

//public
int CdDecoder::getNumTracks()
{
    QMutexLocker lock(&getCdioMutex());

    StCdioDevice cdio(m_devicename);
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

    StCdioDevice cdio(m_devicename);
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
    m_settracknum = track;
    return getMetadata();
}

//public
MusicMetadata *CdDecoder::getLastMetadata()
{
    for(int i = getNumTracks(); i > 0; --i)
    {
        MusicMetadata *m = getMetadata(i);
        if(m)
            return m;
    }
    return NULL;
}

// Create a TOC
static lsn_t s_lastAudioLsn;
static Cddb::Toc& GetToc(CdIo_t *cdio, Cddb::Toc& toc)
{
    // Get lead-in
    const track_t firstTrack = cdio_get_first_track_num(cdio);
    lsn_t lsn0 = 0;
    msf_t msf;
    if (cdio_get_track_msf(cdio, firstTrack, &msf))
        lsn0 = (msf.m * 60 + msf.s) * CDIO_CD_FRAMES_PER_SEC + msf.f;

    const track_t lastTrack = cdio_get_last_track_num(cdio);
    for (track_t t = firstTrack; t <= lastTrack + 1; ++t)
    {
#if 0 // This would be better but the msf's returned are way off in libcdio 0.81
        if (!cdio_get_track_msf(cdio, t, &msf))
            break;
#else
        lsn_t lsn = cdio_get_track_lsn(cdio, t);
        if (s_lastAudioLsn && lsn > s_lastAudioLsn)
            lsn = s_lastAudioLsn;
        lsn += lsn0; // lead-in

        std::div_t d = std::div(lsn, CDIO_CD_FRAMES_PER_SEC);
        msf.f = d.rem;
        d = std::div(d.quot, 60);
        msf.s = d.rem;
        msf.m = d.quot;
#endif
        //LOG(VB_MEDIA, LOG_INFO, QString("Track %1 msf: %2:%3:%4").
        //    arg(t,2).arg(msf.m,2).arg(msf.s,2).arg(msf.f,2) );
        toc.push_back(Cddb::Msf(msf.m, msf.s, msf.f));

        if (TRACK_FORMAT_AUDIO != cdio_get_track_format(cdio, t))
            break;
    }
    return toc;
}

//virtual
MusicMetadata *CdDecoder::getMetadata()
{
    QString artist, album, compilation_artist, title, genre;
    int year = 0;
    unsigned long length = 0;
    track_t tracknum = 0;

    if (-1 == m_settracknum)
        tracknum = getFilename().toUInt();
    else
    {
        tracknum = m_settracknum;
        setFilename(QString("%1" CDEXT).arg(tracknum));
    }

    QMutexLocker lock(&getCdioMutex());

    StCdioDevice cdio(m_devicename);
    if (!cdio)
        return NULL;

    const track_t lastTrack = cdio_get_last_track_num(cdio);
    if (CDIO_INVALID_TRACK == lastTrack)
        return NULL;

    if (TRACK_FORMAT_AUDIO != cdio_get_track_format(cdio, tracknum))
        return NULL;

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
            cdrom_drive_t *dev = cdio_cddap_identify_cdio(cdio, 0, NULL);
            if (NULL != dev)
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
        length = ((end - start + 1) * 1000 + CDIO_CD_FRAMES_PER_SEC/2) /
            CDIO_CD_FRAMES_PER_SEC;
    }

    bool isCompilation = false;

#define CDTEXT 0 // Disabled - cd-text access on discs without it is S L O W
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
        if (NULL != cdtext)
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
                if (NULL != cdtext)
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
        // CDDB lookup
        Cddb::Toc toc;
        Cddb::Matches r;
        if (Cddb::Query(r, GetToc(cdio, toc)))
        {
            Cddb::Matches::match_t::const_iterator select = r.matches.begin();

            if (r.matches.size() > 1)
            {
                // TODO prompt user to select one
                // In the meantime, select the first non-generic genre
                for (Cddb::Matches::match_t::const_iterator it = select;
                    it != r.matches.end(); ++it)
                {
                    QString g = it->discGenre.toLower();
                    if (g != "misc" && g != "data")
                    {
                        select = it;
                        break;
                    }
                }
            }

            Cddb::Album info;
            if (Cddb::Read(info, select->discGenre, select->discID))
            {
                isCompilation = info.isCompilation;

                if (info.genre.toLower() != "unknown")
                    genre = info.genre[0].toTitleCase() + info.genre.mid(1);
                else
                    genre = info.discGenre[0].toTitleCase() + info.discGenre.mid(1);;

                album = info.title;
                compilation_artist = info.artist;
                year = info.year;

                if (info.tracks.size() >= tracknum)
                {
                    const Cddb::Track& track = info.tracks[tracknum - 1];
                    title = track.title;
                    artist = track.artist;
                }

                // Create a temporary local alias for future lookups
                if (r.discID != info.discID)
                    Cddb::Alias(info, r.discID);
            }
        }
    }

    if (compilation_artist.toLower().left(7) == "various")
        compilation_artist = QObject::tr("Various Artists");

    if (artist.isEmpty())
    {
        artist = compilation_artist;
        compilation_artist.clear();
    }

    if (title.isEmpty())
        title = QObject::tr("Track %1").arg(tracknum);

    MusicMetadata *m = new MusicMetadata(getFilename(), artist, compilation_artist,
        album, title, genre, year, tracknum, length);
    if (m)
        m->setCompilation(isCompilation);

    return m;
}

// virtual
void CdDecoder::commitMetadata(MusicMetadata *mdata)
{
    QMutexLocker lock(&getCdioMutex());

    StCdioDevice cdio(m_devicename);
    if (!cdio)
        return;

    Cddb::Toc toc;
    GetToc(cdio, toc);

    unsigned secs;
    Cddb::discid_t discID = Cddb::Discid(secs, toc.data(), toc.size() - 1);

    Cddb::Album album(discID, mdata->Genre().toLower().toUtf8());
    if (!Cddb::Read(album, album.discGenre, discID))
        album.toc = toc;

    album.isCompilation = mdata->Compilation();
    if (!mdata->Compilation())
        album.artist = mdata->Artist();
    else if (mdata->CompilationArtist() != album.artist)
        album.artist = mdata->CompilationArtist();

    album.title = mdata->Album();
    album.year = mdata->Year();

    if (album.tracks.size() < m_tracknum)
        album.tracks.resize(m_tracknum);

    Cddb::Track& track = album.tracks[m_tracknum - 1];
    track.title = mdata->Title();
    track.artist = mdata->Artist();

    Cddb::Write(album);
}


// pure virtual
bool CdDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).toLower() == extension());
}

// pure virtual
const QString &CdDecoderFactory::extension() const
{
    static QString ext(CDEXT);
    return ext;
}

// pure virtual
const QString &CdDecoderFactory::description() const
{
    static QString desc(QObject::tr("Audio CD parser"));
    return desc;
}

// pure virtual
Decoder *CdDecoderFactory::create(const QString &file, AudioOutput *output, bool deletable)
{
   if (deletable)
        return new CdDecoder(file, this, output);

    static CdDecoder *decoder;
    if (! decoder)
    {
        decoder = new CdDecoder(file, this, output);
    }
    else
    {
        decoder->setFilename(file);
        decoder->setOutput(output);
    }

    return decoder;
}

