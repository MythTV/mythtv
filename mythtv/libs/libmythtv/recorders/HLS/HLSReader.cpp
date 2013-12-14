#include <sys/time.h>
#include <unistd.h>

#include "HLSReader.h"

#define LOC QString("%1: ").arg(m_curstream ? m_curstream->Url() : "HLSReader")

static uint64_t MDate(void)
{
    timeval  t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000000ULL + t.tv_usec;
}

HLSReader::HLSReader(void)
    : m_curstream(NULL), m_error(false), m_cancel(false), m_throttle(true),
      m_aesmsg(false), m_playlistworker(NULL), m_streamworker(NULL),
      m_seq_begin(-1), m_seq_first(-1), m_seq_next(-1), m_seq_end(-1),
      m_bandwidthcheck(false), m_prebuffer_cnt(10), m_debug(false), m_slow_cnt(0)
{
}

HLSReader::~HLSReader(void)
{
    Close();
}

bool HLSReader::Open(const QString & m3u)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Opening '%1'").arg(m3u));

    if (IsOpen(m3u))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Already open");
        return true;
    }
    if (m_curstream)
        Close();
    m_cancel = false;

    QByteArray buffer;

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    if (!DownloadURL(m3u, &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Open failed.");
        m_error = true;
        return false;   // can't download file
    }
#else
    MythSingleDownload downloader;
    if (!downloader.DownloadURL(m3u, &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Open failed.");
        m_error = true;
        return false;   // can't download file
    }
#endif

    QTextStream text(&buffer);
    text.setCodec("UTF-8");

    if (!IsValidPlaylist(text))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Open '%1': not a valid playlist").arg(m3u));
        return false;
    }

    m_m3u8 = m3u;

    QMutexLocker lock(&m_stream_lock);
    m_streams.clear();
    m_curstream = NULL;

    if (!ParseM3U8(buffer))
        return false;

    // Select the highest bitrate stream
    StreamContainer::iterator Istream;
    for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
    {
        if (m_curstream == NULL ||
            (*Istream)->Bitrate() > m_curstream->Bitrate())
            m_curstream = *Istream;
    }

    if (!m_curstream)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("No stream selected"));
        return false;
    }
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("Selected stream with %3 bitrate")
        .arg(m_curstream->Bitrate()));

    m_playlistworker = new HLSPlaylistWorker(this);
    m_playlistworker->start();

    m_streamworker = new HLSStreamWorker(this);
    m_streamworker->start();

    LOG(VB_RECORD, LOG_INFO, LOC + "Open -- end");
    return true;
}

void HLSReader::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close -- start");

    Cancel();

    QMutexLocker lock(&m_stream_lock);
    m_curstream = NULL;

    StreamContainer::iterator Istream;
    for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
        delete *Istream;
    m_streams.clear();

    delete m_streamworker;
    m_streamworker = NULL;
    delete m_playlistworker;
    m_playlistworker = NULL;

    LOG(VB_RECORD, LOG_INFO, LOC + "Close -- end");
}

void HLSReader::Cancel(void)
{
    m_cancel = true;

    m_throttle_lock.lock();
    m_throttle_cond.wakeAll();
    m_throttle_lock.unlock();

    QMutexLocker lock(&m_stream_lock);

    if (m_curstream)
        LOG(VB_RECORD, LOG_INFO, LOC + "Cancel");

    if (m_playlistworker)
        m_playlistworker->Cancel();
    if (m_streamworker)
        m_streamworker->Cancel();
#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    uint offset = m_seq_next - m_seq_begin - 1;
    if (m_segments.size() < static_cast<int>(offset))
        CancelURL(m_segments[offset].Url());
#endif
}

void HLSReader::Throttle(bool val)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Throttle(%1)")
        .arg(val ? "true" : "false"));

    m_throttle_lock.lock();
    m_throttle = val;
    if (val)
        m_prebuffer_cnt += 4;
    else
        m_throttle_cond.wakeAll();
    m_throttle_lock.unlock();
}

int HLSReader::Read(uint8_t* buffer, int maxlen)
{
    if (m_error)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Read: in error state");
        return -1;
    }
    if (!m_curstream)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Read: no stream selected");
        return 0;
    }
    if (m_cancel)
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Read: canceled"));
        return 0;
    }

    QMutexLocker lock(&m_buflock);

    int len = m_buffer.size() < maxlen ? m_buffer.size() : maxlen;
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Reading %1 of %2 bytes")
        .arg(len).arg(m_buffer.size()));

    memcpy(buffer, m_buffer.constData(), len);
    if (len < m_buffer.size())
    {
        m_buffer.remove(0, len);
    }
    else
        m_buffer.clear();

    return len;
}

QString HLSReader::DecodedURI(const QString uri)
{
    QByteArray ba   = uri.toLatin1();
    QUrl url        = QUrl::fromEncoded(ba);
    return url.toString();
}

QString HLSReader::RelativeURI(const QString surl, const QString spath)
{
    QUrl url  = QUrl(surl);
    QUrl path = QUrl(spath);

    if (!path.isRelative())
    {
        return spath;
    }
    return url.resolved(path).toString();
}

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
bool HLSReader::DownloadURL(const QString &url, QByteArray *buffer)
{
    MythDownloadManager *mdm = GetMythDownloadManager();
    return mdm->download(url, buffer);
}

void HLSReader::CancelURL(const QString &url)
{
    MythDownloadManager *mdm = GetMythDownloadManager();
    mdm->cancelDownload(url);
    delete mdm;
}

void HLSReader::CancelURL(const QStringList &urls)
{
    MythDownloadManager *mdm = GetMythDownloadManager();
    mdm->cancelDownload(urls);
}
#endif

bool HLSReader::IsValidPlaylist(QTextStream & text)
{
    /* Parse stream and search for
     * EXT-X-TARGETDURATION or EXT-X-STREAM-INF tag, see
     * http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8 */
    QString line = text.readLine();
    if (!line.startsWith((const char*)"#EXTM3U"))
        return false;

    for (;;)
    {
        line = text.readLine();
        if (line.isNull())
            break;
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("IsValidPlaylist: |'%1'").arg(line));
        if (line.startsWith(QLatin1String("#EXT-X-TARGETDURATION"))  ||
            line.startsWith(QLatin1String("#EXT-X-MEDIA-SEQUENCE"))  ||
            line.startsWith(QLatin1String("#EXT-X-KEY"))             ||
            line.startsWith(QLatin1String("#EXT-X-ALLOW-CACHE"))     ||
            line.startsWith(QLatin1String("#EXT-X-ENDLIST"))         ||
            line.startsWith(QLatin1String("#EXT-X-STREAM-INF"))      ||
            line.startsWith(QLatin1String("#EXT-X-DISCONTINUITY"))   ||
            line.startsWith(QLatin1String("#EXT-X-VERSION")))
        {
            return true;
        }
    }

    LOG(VB_RECORD, LOG_ERR, QString("IsValidPlaylist: false"));
    return false;
}

QString HLSReader::ParseAttributes(const QString& line, const char *attr)
{
    int p = line.indexOf(QLatin1String(":"));
    if (p < 0)
        return QString();

    QStringList list = QStringList(line.mid(p + 1).split(','));
    QStringList::iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        QString arg = (*it).trimmed();
        if (arg.startsWith(attr))
        {
            int pos = arg.indexOf(QLatin1String("="));
            if (pos < 0)
                continue;
            return arg.mid(pos+1);
        }
    }
    return QString();
}

/**
 * Return the decimal argument in a line of type: blah:<decimal>
 * presence of valud <decimal> is compulsory or it will return RET_ERROR
 */
bool HLSReader::ParseDecimalValue(const QString& line, int &target)
{
    int p = line.indexOf(QLatin1String(":"));
    if (p < 0)
        return false;
    int i = p;
    while (++i < line.size() && line[i].isNumber());
    if (i == p + 1)
        return false;
    target = line.mid(p + 1, i - p - 1).toInt();
    return true;
}

/**
 * Return the decimal argument in a line of type: blah:<decimal>
 * presence of valud <decimal> is compulsory or it will return RET_ERROR
 */
bool HLSReader::ParseDecimalValue(const QString& line, int64_t &target)
{
    int p = line.indexOf(QLatin1String(":"));
    if (p < 0)
        return false;
    int i = p;
    while (++i < line.size() && line[i].isNumber());
    if (i == p + 1)
        return false;
    target = line.mid(p + 1, i - p - 1).toInt();
    return true;
}

bool HLSReader::ParseVersion(const QString& line, int &version,
                             const QString& loc)
{
    /*
     * The EXT-X-VERSION tag indicates the compatibility version of the
     * Playlist file.  The Playlist file, its associated media, and its
     * server MUST comply with all provisions of the most-recent version of
     * this document describing the protocol version indicated by the tag
     * value.
     *
     * Its format is:
     *
     * #EXT-X-VERSION:<n>
     */

    if (line.isNull() || !ParseDecimalValue(line, version))
    {
        LOG(VB_RECORD, LOG_ERR, loc +
            "#EXT-X-VERSION: no protocol version found, should be version 1.");
        version = 1;
        return false;
    }

    if (version <= 0 || version > 3)
    {
        LOG(VB_RECORD, LOG_ERR, loc +
            QString("#EXT-X-VERSION is %1, but we only understand 0 through 3")
            .arg(version));
        return false;
    }

    return true;
}

HLSRecStream* HLSReader::ParseStreamInformation(const QString& line,
                                                const QString& url,
                                                const QString& loc)
{
    /*
     * #EXT-X-STREAM-INF:[attribute=value][,attribute=value]*
     *  <URI>
     */
    int id;
    uint64_t bw;
    QString attr;

    attr = ParseAttributes(line, "PROGRAM-ID");
    if (attr.isNull())
    {
        LOG(VB_RECORD, LOG_INFO, loc +
            "#EXT-X-STREAM-INF: expected PROGRAM-ID=<value>, using -1");
        id = -1;
    }
    else
    {
        id = attr.toInt();
    }

    attr = ParseAttributes(line, "BANDWIDTH");
    if (attr.isNull())
    {
        LOG(VB_RECORD, LOG_ERR, loc +
            "#EXT-X-STREAM-INF: expected BANDWIDTH=<value>");
        return NULL;
    }
    bw = attr.toInt();

    if (bw == 0)
    {
        LOG(VB_RECORD, LOG_ERR, loc +
            "#EXT-X-STREAM-INF: bandwidth cannot be 0");
        return NULL;
    }

    LOG(VB_RECORD, LOG_INFO, loc +
        QString("bandwidth adaptation detected (program-id=%1, bandwidth=%2")
        .arg(id).arg(bw));

    return new HLSRecStream(id, bw, url);
}

bool HLSReader::ParseTargetDuration(HLSRecStream* hls, const QString& line,
                                    const QString& loc)
{
    /*
     * #EXT-X-TARGETDURATION:<s>
     *
     * where s is an integer indicating the target duration in seconds.
     */
    int duration = -1;

    if (!ParseDecimalValue(line, duration))
    {
        LOG(VB_RECORD, LOG_ERR, loc + "expected #EXT-X-TARGETDURATION:<s>");
        return false;
    }
    hls->SetTargetDuration(duration); /* seconds */

    return true;
}

bool HLSReader::ParseSegmentInformation(const HLSRecStream* hls,
                                        const QString& line, uint& duration,
                                        QString& title, const QString& loc)
{
    /*
     * #EXTINF:<duration>,<title>
     *
     * "duration" is an integer that specifies the duration of the media
     * file in seconds.  Durations SHOULD be rounded to the nearest integer.
     * The remainder of the line following the comma is the title of the
     * media file, which is an optional human-readable informative title of
     * the media segment
     */
    int p = line.indexOf(QLatin1String(":"));
    if (p < 0)
    {
        LOG(VB_RECORD, LOG_ERR, loc +
            QString("ParseSegmentInformation: Missing ':' in '%1'")
            .arg(line));
        return false;
    }

    QStringList list = QStringList(line.mid(p + 1).split(','));

    /* read duration */
    if (list.isEmpty())
    {
        LOG(VB_RECORD, LOG_ERR, loc +
            QString("ParseSegmentInformation: Missing arguments in '%1'")
            .arg(line));
        return false;
    }

    QString val = list[0];
    bool ok;

    if (hls->Version() < 3)
    {
        duration = val.toInt(&ok);
        if (!ok)
        {
            duration = -1;
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("ParseSegmentInformation: invalid duration in '%1'")
                .arg(line));
        }
    }
    else
    {
        double d = val.toDouble(&ok);
        if (!ok)
        {
            duration = -1;
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("ParseSegmentInformation: invalid duration in '%1'")
                .arg(line));
            return false;
        }
        if ((d) - ((int)d) >= 0.5)
            duration = ((int)d) + 1;
        else
            duration = ((int)d);
    }

    if (list.size() >= 2)
    {
        title = list[1];
    }

    /* Ignore the rest of the line */
    return true;
}

bool HLSReader::ParseMediaSequence(int64_t & sequence_num,
                                   const QString& line, const QString& loc)
{
    /*
     * #EXT-X-MEDIA-SEQUENCE:<number>
     *
     * A Playlist file MUST NOT contain more than one EXT-X-MEDIA-SEQUENCE
     * tag.  If the Playlist file does not contain an EXT-X-MEDIA-SEQUENCE
     * tag then the sequence number of the first URI in the playlist SHALL
     * be considered to be 0.
     */

    if (!ParseDecimalValue(line, sequence_num))
    {
        LOG(VB_RECORD, LOG_ERR, loc + "expected #EXT-X-MEDIA-SEQUENCE:<s>");
        return false;
    }

    return true;
}

bool HLSReader::ParseKey(HLSRecStream* hls, const QString& line,
                         bool& aesmsg, const QString& loc)
{
    /*
     * #EXT-X-KEY:METHOD=<method>[,URI="<URI>"][,IV=<IV>]
     *
     * The METHOD attribute specifies the encryption method.  Two encryption
     * methods are defined: NONE and AES-128.
     */

    QString attr = ParseAttributes(line, "METHOD");
    if (attr.isNull())
    {
        LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: expected METHOD=<value>");
        return false;
    }

    if (attr.startsWith(QLatin1String("NONE")))
    {
        QString uri = ParseAttributes(line, "URI");
        if (!uri.isNull())
        {
            LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: URI not expected");
            return false;
        }
        /* IV is only supported in version 2 and above */
        if (hls->Version() >= 2)
        {
            QString iv = ParseAttributes(line, "IV");
            if (!iv.isNull())
            {
                LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: IV not expected");
                return false;
            }
        }
    }
#ifdef USING_LIBCRYPTO
    else if (attr.startsWith(QLatin1String("AES-128")))
    {
        QString uri, iv;
        if (aesmsg == false)
        {
            LOG(VB_RECORD, LOG_INFO, loc +
                "playback of AES-128 encrypted HTTP Live media detected.");
            aesmsg = true;
        }
        uri = ParseAttributes(line, "URI");
        if (uri.isNull())
        {
            LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: URI not found for "
                "encrypted HTTP Live media in AES-128");
            return false;
        }

        /* Url is between quotes, remove them */
        hls->SetKeyPath(DecodedURI(uri.remove(QChar(QLatin1Char('"')))));

        iv = ParseAttributes(line, "IV");
        if (!iv.isNull() && !hls->SetAESIV(iv))
        {
            LOG(VB_RECORD, LOG_ERR, loc + "invalid IV");
            return false;
        }
    }
#endif
    else
    {
#ifndef _MSC_VER
        LOG(VB_RECORD, LOG_ERR, loc +
            "invalid encryption type, only NONE "
#ifdef USING_LIBCRYPTO
            "and AES-128 are supported"
#else
            "is supported."
#endif
            );
#else
// msvc doesn't like #ifdef in the middle of the LOG macro.
// Errors with '#':invalid character: possibly the result of a macro expansion
#endif
        return false;
    }
    return true;
}

bool HLSReader::ParseProgramDateTime(HLSRecStream* hls, const QString& line,
                                     const QString& loc)
{
    /*
     * #EXT-X-PROGRAM-DATE-TIME:<YYYY-MM-DDThh:mm:ssZ>
     */
    LOG(VB_RECORD, LOG_DEBUG, loc +
        QString("tag not supported: #EXT-X-PROGRAM-DATE-TIME %1")
        .arg(line));
    return true;
}

bool HLSReader::ParseAllowCache(HLSRecStream* hls, const QString& line,
                                const QString& loc)
{
    /*
     * The EXT-X-ALLOW-CACHE tag indicates whether the client MAY or MUST
     * NOT cache downloaded media files for later replay.  It MAY occur
     * anywhere in the Playlist file; it MUST NOT occur more than once.  The
     * EXT-X-ALLOW-CACHE tag applies to all segments in the playlist.  Its
     * format is:
     *
     * #EXT-X-ALLOW-CACHE:<YES|NO>
     */

    int pos = line.indexOf(QLatin1String(":"));
    if (pos < 0)
    {
        LOG(VB_RECORD, LOG_ERR, loc +
            QString("ParseAllowCache: missing ':' in '%1'")
            .arg(line));
        return false;
    }
    QString answer = line.mid(pos+1, 3);
    if (answer.size() < 2)
    {
        LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-ALLOW-CACHE, ignoring ...");
        return true;
    }
    hls->SetCache(!answer.startsWith(QLatin1String("NO")));

    return true;
}

bool HLSReader::ParseDiscontinuity(HLSRecStream* hls, const QString& line,
                                   const QString& loc)
{
    /* Not handled, never seen so far */
    LOG(VB_RECORD, LOG_DEBUG, loc + QString("#EXT-X-DISCONTINUITY %1")
        .arg(line));
    return true;
}

bool HLSReader::ParseEndList(HLSRecStream* hls, const QString& loc)
{
    /*
     * The EXT-X-ENDLIST tag indicates that no more media files will be
     * added to the Playlist file.  It MAY occur anywhere in the Playlist
     * file; it MUST NOT occur more than once.  Its format is:
     */
    hls->SetLive(false);
    LOG(VB_RECORD, LOG_INFO, loc + "video on demand (vod) mode");
    return true;
}

bool HLSReader::ParseM3U8(const QByteArray& buffer, HLSRecStream* stream)
{
    /**
     * The http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8
     * document defines the following new tags: EXT-X-TARGETDURATION,
     * EXT-X-MEDIA-SEQUENCE, EXT-X-KEY, EXT-X-PROGRAM-DATE-TIME, EXT-X-
     * ALLOW-CACHE, EXT-X-STREAM-INF, EXT-X-ENDLIST, EXT-X-DISCONTINUITY,
     * and EXT-X-VERSION.
     */

    LOG(VB_RECORD, LOG_DEBUG, LOC + "ParseM3U8 -- begin");

    QTextStream text(buffer);

    QString line = text.readLine();
    if (line.isNull())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "ParseM3U8: empty line");
        return false;
    }

    if (!line.startsWith(QLatin1String("#EXTM3U")))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "ParseM3U8: missing #EXTM3U tag .. aborting");
        return false;
    }

    /* What is the version ? */
    int version = 1;
    int p = buffer.indexOf("#EXT-X-VERSION:");
    if (p >= 0)
    {
        text.seek(p);
        if (!ParseVersion(text.readLine(), version, StreamURL()))
            return false;
    }

    if (buffer.indexOf("#EXT-X-STREAM-INF") >= 0)
    {
        // Meta index file
        LOG(VB_RECORD, LOG_INFO, LOC + "Meta index file");

        /* M3U8 Meta Index file */
        text.seek(0); // rewind
        while (!m_cancel)
        {
            line = text.readLine();
            if (line.isNull())
                break;

            LOG(VB_RECORD, LOG_DEBUG, LOC + QString("|%1").arg(line));
            if (line.startsWith(QLatin1String("#EXT-X-STREAM-INF")))
            {
                QString uri = text.readLine();
                if (uri.isNull() || uri.startsWith(QLatin1String("#")))
                {
                    LOG(VB_RECORD, LOG_INFO, LOC +
                        QString("ParseM3U8: Invalid EXT-X-STREAM-INF data '%1'")
                        .arg(uri));
                }
                else
                {
                    StreamContainer::iterator Istream;
                    QString url = RelativeURI(m_m3u8, DecodedURI(uri));

                    if ((Istream = m_streams.find(url)) == m_streams.end())
                    {
                        HLSRecStream *hls = ParseStreamInformation(line, url,
                                                                   StreamURL());
                        if (hls)
                        {
                            LOG(VB_RECORD, LOG_INFO, LOC +
                                QString("Adding stream %1")
                                .arg(hls->toString()));
                            Istream = m_streams.insert(url, hls);
                        }
                    }
                    else
                        LOG(VB_RECORD, LOG_INFO, LOC +
                            QString("Already have stream '%1'").arg(url));
                }
            }
        }
    }
    else
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + "Meta playlist");

        HLSRecStream *hls;
        if (stream)
            hls = stream;
        else
        {
            /* No Meta playlist used */
            StreamContainer::iterator Istream;

            if ((Istream = m_streams.find(DecodedURI(m_m3u8))) ==
                m_streams.end())
            {
                hls = new HLSRecStream(0, 0, m_m3u8);
                if (hls)
                {
                    LOG(VB_RECORD, LOG_INFO, LOC +
                        QString("Adding new stream '%1'").arg(m_m3u8));
                    Istream = m_streams.insert(m_m3u8, hls);
                }
            }
            else
            {
                hls = *Istream;
                LOG(VB_RECORD, LOG_INFO, LOC +
                    QString("Updating stream '%1'").arg(hls->toString()));
            }

            /* Get TARGET-DURATION first */
            p = buffer.indexOf("#EXT-X-TARGETDURATION:");
            if (p >= 0)
            {
                text.seek(p);
                if (!ParseTargetDuration(hls, text.readLine(), StreamURL()))
                    return false;
            }
            /* Store version */
            hls->SetVersion(version);
        }
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("%1 Playlist HLS protocol version: %2")
            .arg(hls->Live() ? "Live": "VOD").arg(version));

        // rewind
        text.seek(0);

        QString title;
        uint    segment_duration = -1;
        int64_t first_sequence   = -1;
        int64_t sequence_num     = 0;

        QMutexLocker lock(&m_seq_lock);
        while (!m_cancel)
        {
            /* Next line */
            line = text.readLine();
            if (line.isNull())
                break;
            LOG(VB_RECORD, LOG_DEBUG, LOC + QString("|%1").arg(line));

            if (line.startsWith(QLatin1String("#EXTINF")))
            {
                if (!ParseSegmentInformation(hls, line, segment_duration,
                                             title, StreamURL()))
                    return false;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-TARGETDURATION")))
            {
                if (!ParseTargetDuration(hls, line, StreamURL()))
                    return false;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-MEDIA-SEQUENCE")))
            {
                if (!ParseMediaSequence(sequence_num, line, StreamURL()))
                    return false;
                if (first_sequence < 0)
                {
                    if (m_seq_end >= 0 && m_seq_end + 3 < sequence_num)
                    {
                        // Sequence range has jumpted to a much higher number
                        LOG(VB_RECORD, LOG_WARNING, LOC +
                            QString("Resetting sequence from %1 to %2")
                            .arg(m_seq_next).arg(sequence_num));
                        m_seq_next = sequence_num;
                    }

                    m_seq_first = first_sequence = sequence_num;
                    if (m_seq_next < 0)
                        m_seq_next = sequence_num;
                    if (m_seq_next < first_sequence)
                    {
                        m_seq_begin = m_seq_next;
                        m_segments.erase(m_segments.begin(),
                                         m_segments.begin() +
                                         (m_seq_begin -
                                          m_segments.front().Sequence()));
                    }
                    else
                        m_seq_begin = first_sequence;
                }
            }
            else if (line.startsWith(QLatin1String("#EXT-X-KEY")))
            {
                if (!ParseKey(hls, line, m_aesmsg, StreamURL()))
                    return false;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-PROGRAM-DATE-TIME")))
            {
                if (!ParseProgramDateTime(hls, line, StreamURL()))
                    return false;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-ALLOW-CACHE")))
            {
                if (!ParseAllowCache(hls, line, StreamURL()))
                    return false;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-DISCONTINUITY")))
            {
                if (!ParseDiscontinuity(hls, line, StreamURL()))
                    return false;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-VERSION")))
            {
                int version;
                if (!ParseVersion(line, version, StreamURL()))
                    return false;
                hls->SetVersion(version);
            }
            else if (line.startsWith(QLatin1String("#EXT-X-ENDLIST")))
            {
                if (!ParseEndList(hls, StreamURL()))
                    return false;
            }
            else if (!line.startsWith(QLatin1String("#")) && !line.isEmpty())
            {
                UpdateSegment(sequence_num++, segment_duration,
                              title, RelativeURI(hls->Url(), DecodedURI(line)));
                segment_duration = -1; /* reset duration */
                title.clear();
            }
        }

        m_seq_end = sequence_num;
        if (m_seq_next > m_seq_end)
        {
            // Sequence range has jumpted to a lower number
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Resetting sequence from %1 to %2")
                .arg(m_seq_next).arg(m_seq_begin));
            m_seq_next = m_seq_first;
        }

        if (m_seq_next < m_seq_first)
        {
            int max_behind = (m_seq_end - m_seq_first) / 2;
            int behind = m_seq_first - m_seq_next;
            if (max_behind < behind)
            {
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    QString("Not downloading fast enough! "
                            "%1 segments behind, skipping %2 segments")
                    .arg(behind).arg(behind - max_behind));
                m_seq_next = m_seq_first - max_behind;
                m_streamworker->CancelCurrentDownload();
                m_debug = true;
            }
        }
    }

    LOG(VB_RECORD, LOG_DEBUG, LOC + "ParseM3U8 -- end");
    return true;
}

bool HLSReader::LoadMetaPlaylists(MythSingleDownload& downloader)
{
    if (!m_curstream || m_cancel)
        return false;

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("LoadMetaPlaylists stream %1")
        .arg(m_curstream->toString()));

    StreamContainer::iterator   Istream;

    m_stream_lock.lock();
    if (m_bandwidthcheck && m_seq_end > -1)
    {
        int buffered = PercentBuffered();
        if (buffered < 15)
        {
            // It is taking too long to download the segments
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Falling behind: only %1% buffered").arg(buffered));
            DecreaseBitrate(m_curstream->Id());
            m_bandwidthcheck = false;
        }
        else if (buffered > 85)
        {
            // Keeping up easily, raise the bitrate.
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Plenty of bandwidth, downloading %1 in range %2 - %3")
                .arg(m_seq_next - 1).arg(m_seq_first).arg(m_seq_end - 1));
            IncreaseBitrate(m_curstream->Id());
            m_bandwidthcheck = false;
        }
    }
    m_stream_lock.unlock();

    QByteArray buffer;

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    if (!DownloadURL(m_curstream->Url(), &buffer))
        return false;
#else
    if (!downloader.DownloadURL(m_curstream->Url(), &buffer))
        return false;
#endif

    if (m_cancel || !ParseM3U8(buffer, m_curstream))
        return false;

    // Signal SegmentWorker that there is work to do...
    m_streamworker->Wakeup();

    return true;
}

bool HLSReader::UpdateSegment(int64_t sequence_num, int duration,
                              const QString& title, const QString& url)
{
    int64_t offset = sequence_num - m_seq_begin;

    if (m_segments.size() == offset)
    {
        if (m_seq_end < 0)
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("UpdateSegment appending new segment [%1] %2 secs %3 %4")
                .arg(sequence_num).arg(duration).arg(title).arg(url));
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("%1 buffer segments").arg(m_segments.size()));
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("UpdateSegment appending new segment [%1] %2 secs %3 %4")
                .arg(sequence_num).arg(duration).arg(title).arg(url));
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("%1 buffer segments, %2 in use")
                .arg(m_segments.size()).arg(m_seq_end - m_seq_begin));
        }

        if (m_seq_end > 0)
        {
            // Sanity check
            if (m_segments.last().Sequence() + 1 != sequence_num)
            {
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    QString("Sequence number gap %1 - %2")
                    .arg(m_segments[m_seq_end - 1].Sequence())
                    .arg(sequence_num));
            }
        }
        m_segments.push_back(HLSRecSegment(sequence_num, duration,
                                           title, url));
    }
    else if (m_segments.size() < offset)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("UpdateSegment invalid offset %1 for "
                    "%1 begin: %2 first: %3 next: %4 end: %5")
            .arg(offset).arg(sequence_num).arg(m_seq_begin).arg(m_seq_first)
            .arg(m_seq_next).arg(m_seq_end));
    }
    else
    {
        m_segments[offset].m_sequence = sequence_num;
        m_segments[offset].m_duration = duration;
        m_segments[offset].m_title = title;
        m_segments[offset].m_url = url;
        if (sequence_num > m_seq_next)
            m_segments[offset].m_bitrate = 0;
    }

    return true;
}

void HLSReader::DecreaseBitrate(int progid)
{
    HLSRecStream *hls = NULL;
    uint64_t bitrate = m_curstream->Bitrate();
    uint64_t candidate = 0;
    StreamContainer::const_iterator Istream;

    for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
    {
        if ((*Istream)->Id() != progid)
            continue;
        if (bitrate > (*Istream)->Bitrate() &&
            candidate < (*Istream)->Bitrate())
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("candidate stream '%1' bitrate %2 >= %3")
                .arg(Istream.key()).arg(bitrate).arg((*Istream)->Bitrate()));
            hls = *Istream;
            candidate = hls->Bitrate();
        }
    }

    if (hls)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Switching to a lower bitrate stream %1 -> %2")
            .arg(bitrate).arg(candidate));
        QMutexLocker lock(&m_stream_lock);
        m_curstream = hls;
    }
    else
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Already at lowest bitrate %1").arg(bitrate));
    }
}

void HLSReader::IncreaseBitrate(int progid)
{
    HLSRecStream *hls = NULL;
    uint64_t bitrate = m_curstream->Bitrate();
    uint64_t candidate = INT_MAX;
    StreamContainer::const_iterator Istream;

    for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
    {
        if ((*Istream)->Id() != progid)
            continue;
        if (bitrate < (*Istream)->Bitrate() &&
            candidate > (*Istream)->Bitrate())
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("candidate stream '%1' bitrate %2 >= %3")
                .arg(Istream.key()).arg(bitrate).arg((*Istream)->Bitrate()));
            hls = *Istream;
            candidate = hls->Bitrate();
        }
    }

    if (hls)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Switching to a higher bitrate stream %1 -> %2")
            .arg(bitrate).arg(candidate));
        QMutexLocker lock(&m_stream_lock);
        m_curstream = hls;
    }
    else
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Already at highest bitrate %1").arg(bitrate));
    }
}

bool HLSReader::LoadSegments(MythSingleDownload& downloader)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "LoadSegment -- start");

    if (!m_curstream)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "LoadSegment: current stream not set.");
        m_bandwidthcheck = true;
        return false;
    }

    if (m_seq_end < 0)
        return true;

    HLSRecStream *hls;
    HLSRecSegment seg;
    int           throttle;
    int           playlist_size = m_seq_end - m_seq_begin;
    for (;;)
    {
        m_seq_lock.lock();
        if (m_cancel || m_seq_next == m_seq_end)
        {
            m_seq_lock.unlock();
            break;
        }

        int64_t offset = m_seq_next - m_seq_begin;
        seg = m_segments[offset];
        ++m_seq_next;
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Downloading segment %1, begin: %2 "
                    "first %3: next: %4 end: %5")
            .arg(seg.Sequence()).arg(m_seq_begin).arg(m_seq_first)
            .arg(m_seq_next).arg(m_seq_end));
        m_seq_lock.unlock();

        m_stream_lock.lock();
        hls = m_curstream;
        m_stream_lock.unlock();

        throttle = DownloadSegmentData(downloader, hls,
                                       seg, playlist_size);

        if (m_throttle && throttle == 0)
            throttle = 5;
        if (throttle > 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Throttling -- sleeping %1 secs.")
                .arg(throttle));
            m_throttle_lock.lock();
            m_throttle_cond.wait(&m_throttle_lock, 1000000 * throttle);
            m_throttle_lock.unlock();
        }
        else
            usleep(500);

        if (m_prebuffer_cnt == 0)
        {
            m_bandwidthcheck = true;
            m_prebuffer_cnt = 2;
        }
        else
            --m_prebuffer_cnt;
    }

    LOG(VB_RECORD, LOG_DEBUG, LOC + "LoadSegment -- end");
    return true;
}

uint HLSReader::PercentBuffered(void) const
{
    if (m_seq_next < m_seq_first)
        return 0;
    return (static_cast<float>(m_seq_next - m_seq_first) /
            static_cast<float>(m_seq_end - m_seq_first) * 100.0);
}

int HLSReader::DownloadSegmentData(MythSingleDownload& downloader,
                                   HLSRecStream* hls,
                                   HLSRecSegment& segment, int playlist_size)
{
    uint64_t bandwidth = hls->AverageBandwidth();
    int estimated_time = 0;

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Downloading %1 bandwidth %2 bitrate %3")
        .arg(segment.Sequence()).arg(bandwidth).arg(hls->Bitrate()));

    /* sanity check - can we download this segment on time? */
    if ((bandwidth > 0) && (hls->Bitrate() > 0))
    {
        uint64_t size = (segment.Duration() * hls->Bitrate()); /* bits */
        estimated_time = (int)(size / bandwidth);
        if (estimated_time > segment.Duration())
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("downloading of %1 will take %2s, "
                        "which is longer than its playback (%3s) at %4kiB/s")
                .arg(segment.Sequence())
                .arg(estimated_time)
                .arg(segment.Duration())
                .arg(bandwidth / 8192));
        }
    }

    QByteArray buffer;
    uint64_t start = MDate();

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
                                   // and can only handle six download at a time
    if (!HLSReader::DownloadURL(segment.Url(), &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("%1 failed").arg(segment.Sequence()));
        if (estimated_time * 2 < segment.Duration())
        {
            if (!HLSReader::DownloadURL(segment.Url(), &buffer))
                return 0;
        }
        else
            return 0;
    }
#else
    if (!downloader.DownloadURL(segment.Url(), &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("%1 failed")
            .arg(segment.Sequence()));
        if (estimated_time * 2 < segment.Duration())
        {
            if (!downloader.DownloadURL(segment.Url(), &buffer))
                return 0;
        }
        else
            return 0;
    }
#endif

    uint64_t downloadduration = (MDate() - start) / 1000;

#ifdef USING_LIBCRYPTO
    /* If the segment is encrypted, decode it */
    if (segment.HasKeyPath())
    {
        if (!hls->DecodeData(downloader, hls->IVLoaded() ? hls->AESIV() : NULL,
                             segment.KeyPath(),
                             buffer, segment.Sequence()));
        return 0;
    }
#endif

    int segment_len = buffer.size();

    m_buflock.lock();
    if (m_buffer.size() > segment_len * playlist_size)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("streambuffer is not reading fast enough. "
                    "buffer size %1").arg(m_buffer.size()));
        if (++m_slow_cnt > 10)
            m_slow_cnt = 10;
    }
    else if (m_slow_cnt > 0)
        --m_slow_cnt;

    if (m_buffer.size() >= segment_len * playlist_size * 2)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("streambuffer is not reading fast enough. "
                    "buffer size %1.  Dropping %2 bytes")
            .arg(m_buffer.size()).arg(segment_len));
        m_buffer.remove(0, segment_len);
    }

    m_buffer += buffer;
    m_buflock.unlock();

    if (hls->Bitrate() == 0 && segment.Duration() > 0)
    {
        /* Try to estimate the bandwidth for this stream */
        hls->SetBitrate((uint64_t)(((double)segment_len * 8) /
                                   ((double)segment.Duration())));
    }

    if (downloadduration < 1)
        downloadduration = 1;

    /* bits/sec */
    bandwidth = segment_len * 8 * 1000ULL / downloadduration;
    hls->AverageBandwidth(bandwidth);
    hls->SetCurrentByteRate(static_cast<uint64_t>
                            ((static_cast<double>(segment_len) /
                              static_cast<double>(segment.Duration()))));

    LOG(VB_RECORD, (m_debug ? LOG_INFO : LOG_DEBUG), LOC +
        QString("%1 took %3ms for %4 bytes: "
                "bandwidth:%5kiB/s")
        .arg(segment.Sequence())
        .arg(downloadduration)
        .arg(segment_len)
        .arg(bandwidth / 8192.0));

    return m_slow_cnt;
}
