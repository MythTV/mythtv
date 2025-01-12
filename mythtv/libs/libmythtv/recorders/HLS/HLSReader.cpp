#include <sys/time.h>
#include <unistd.h>

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif

#include "HLSReader.h"
#include "HLS/m3u.h"

#define LOC QString("HLSReader[%1]: ").arg(m_inputId)

/**
 * Handles relative URLs without breaking URI encoded parameters by avoiding
 * storing the decoded URL in a QString.
 * It replaces M3U::RelativeURI("", M3U::DecodedURI(your segment URL here));
 */
static QUrl RelativeURI (const QString& baseString, const QString& uriString)
{
    QUrl base(baseString);
    QUrl uri(QUrl::fromEncoded(uriString.toLatin1()));

    return base.resolved(uri);
}

HLSReader::~HLSReader(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "dtor -- start");
    Close();
    LOG(VB_RECORD, LOG_INFO, LOC + "dtor -- end");
}

bool HLSReader::Open(const QString & m3u, int bitrate_index)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Opening '%1'").arg(m3u));

    m_bitrateIndex = bitrate_index;

    if (IsOpen(m3u))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Already open");
        return true;
    }
    Close(true);
    m_cancel = false;

    QByteArray buffer;

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    if (!DownloadURL(m3u, &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Open failed.");
        return false;   // can't download file
    }
#else
    MythSingleDownload downloader;
    QString redir;
    if (!downloader.DownloadURL(m3u, &buffer, 30s, 0, 0, &redir))
    {
        LOG(VB_GENERAL, LOG_ERR,
            LOC + "Open failed: " + downloader.ErrorString());
        return false;   // can't download file
    }
#endif

    QTextStream text(&buffer);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    text.setCodec("UTF-8");
#else
    text.setEncoding(QStringConverter::Utf8);
#endif

    if (!IsValidPlaylist(text))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Open '%1': not a valid playlist").arg(m3u));
        return false;
    }

    m_m3u8 = m3u;
    m_segmentBase = redir.isEmpty() ? m3u : redir;

    QMutexLocker lock(&m_streamLock);
    m_streams.clear();
    m_curstream = nullptr;

    if (!ParseM3U8(buffer))
        return false;

    if (m_bitrateIndex == 0)
    {
        // Select the highest bitrate stream
        StreamContainer::iterator Istream;
        for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
        {
            if (m_curstream == nullptr ||
                (*Istream)->Bitrate() > m_curstream->Bitrate())
                m_curstream = *Istream;
        }
    }
    else
    {
        if (m_streams.size() < m_bitrateIndex)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Open '%1': Only %2 bitrates, %3 is not a valid index")
                .arg(m3u)
                .arg(m_streams.size())
                .arg(m_bitrateIndex));
            m_fatal = true;
            return false;
        }

        auto it = m_streams.begin();
        for (auto i = 0; i < m_bitrateIndex - 1; i++)
            it++;
        m_curstream = *it;
    }

    if (!m_curstream)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("No stream selected"));
        m_fatal = true;
        return false;
    }
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("Selected stream with %3 bitrate")
        .arg(m_curstream->Bitrate()));

    QMutexLocker worker_lock(&m_workerLock);

    m_playlistWorker = new HLSPlaylistWorker(this);
    m_playlistWorker->start();

    m_streamWorker = new HLSStreamWorker(this);
    m_streamWorker->start();

    LOG(VB_RECORD, LOG_INFO, LOC + "Open -- end");
    return true;
}

void HLSReader::Close(bool quiet)
{
    LOG(VB_RECORD, (quiet ? LOG_DEBUG : LOG_INFO), LOC + "Close -- start");

    Cancel(quiet);

    QMutexLocker stream_lock(&m_streamLock);
    m_curstream = nullptr;

    StreamContainer::iterator Istream;
    for (Istream = m_streams.begin(); Istream != m_streams.end(); ++Istream)
        delete *Istream;
    m_streams.clear();

    QMutexLocker lock(&m_workerLock);

    delete m_streamWorker;
    m_streamWorker = nullptr;
    delete m_playlistWorker;
    m_playlistWorker = nullptr;

    LOG(VB_RECORD, (quiet ? LOG_DEBUG : LOG_INFO), LOC + "Close -- end");
}

void HLSReader::Cancel(bool quiet)
{
    LOG(VB_RECORD, (quiet ? LOG_DEBUG : LOG_INFO), LOC + "Cancel -- start");

    m_cancel = true;

    m_throttleLock.lock();
    m_throttleCond.wakeAll();
    m_throttleLock.unlock();

    QMutexLocker lock(&m_workerLock);

    if (m_curstream)
        LOG(VB_RECORD, LOG_INFO, LOC + "Cancel");

    if (m_playlistWorker)
        m_playlistWorker->Cancel();

    if (m_streamWorker)
        m_streamWorker->Cancel();

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    if (!m_sements.empty())
        CancelURL(m_segments.front().Url());
#endif

    LOG(VB_RECORD, (quiet ? LOG_DEBUG : LOG_INFO), LOC + "Cancel -- done");
}

void HLSReader::Throttle(bool val)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Throttle(%1)")
        .arg(val ? "true" : "false"));

    m_throttleLock.lock();
    m_throttle = val;
    if (val)
        m_prebufferCnt += 4;
    else
        m_throttleCond.wakeAll();
    m_throttleLock.unlock();
}

qint64 HLSReader::Read(uint8_t* buffer, qint64 maxlen)
{
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

    QMutexLocker lock(&m_bufLock);

    qint64 len = m_buffer.size() < maxlen ? m_buffer.size() : maxlen;
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Reading %1 of %2 bytes")
        .arg(len).arg(m_buffer.size()));

    memcpy(buffer, m_buffer.constData(), len);
    if (len < m_buffer.size())
    {
        m_buffer.remove(0, len);
    }
    else
    {
        m_buffer.clear();
    }

    return len;
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
     * http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8
     *
     * Updated with latest available version from 2017, see
     * https://datatracker.ietf.org/doc/html/rfc8216
     */
    QString line = text.readLine();
    if (!line.startsWith((const char*)"#EXTM3U"))
        return false;

    for (;;)
    {
        line = text.readLine();
        if (line.isNull())
            break;
        LOG(VB_RECORD, LOG_DEBUG,
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

    // Version is 1 if not specified, otherwise must be in range 1 to 7.
    int version = 1;
    int p = buffer.indexOf("#EXT-X-VERSION:");
    if (p >= 0)
    {
        text.seek(p);
        if (!M3U::ParseVersion(text.readLine(), StreamURL(), version))
            return false;
    }

    if (buffer.indexOf("#EXT-X-STREAM-INF") >= 0)
    {
        // Meta index file
        LOG(VB_RECORD, LOG_DEBUG, LOC + "Master Playlist");

        /* M3U8 Meta Index file */
        text.seek(0); // rewind
        while (!m_cancel)
        {
            line = text.readLine();
            if (line.isNull())
                break;

            LOG(VB_RECORD, LOG_INFO, LOC + QString("|%1").arg(line));
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
                    QString url = RelativeURI(m_segmentBase, uri).toString();

                    StreamContainer::iterator Istream = m_streams.find(url);
                    if (Istream == m_streams.end())
                    {
                        int id = 0;
                        uint64_t bandwidth = 0;
                        if (!M3U::ParseStreamInformation(line, url, StreamURL(),
                                                         id, bandwidth))
                            break;
                        auto *hls = new HLSRecStream(id, bandwidth, url,
                                                     m_segmentBase);
                        if (hls)
                        {
                            LOG(VB_RECORD, LOG_INFO, LOC +
                                QString("Adding stream %1")
                                .arg(hls->toString()));
                            Istream = m_streams.insert(url, hls);
                        }
                    }
                    else
                    {
                        LOG(VB_RECORD, LOG_INFO, LOC +
                            QString("Already have stream '%1'").arg(url));
                    }
                }
            }
        }
    }
    else
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + "Media Playlist");

        HLSRecStream *hls = stream;
        if (stream == nullptr)
        {
            /* No Meta playlist used */
            StreamContainer::iterator Istream =
                m_streams.find(M3U::DecodedURI(m_m3u8));
            if (Istream == m_streams.end())
            {
                hls = new HLSRecStream(0, 0, m_m3u8, m_segmentBase);
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
                int duration = 0;

                text.seek(p);
                if (!M3U::ParseTargetDuration(text.readLine(), StreamURL(),
                                              duration))
                    return false;
                hls->SetTargetDuration(std::chrono::seconds(duration));
            }
            /* Store version */
            hls->SetVersion(version);
        }
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("%1 Media Playlist HLS protocol version: %2")
            .arg(hls->Live() ? "Live": "VOD").arg(version));

        // rewind
        text.seek(0);

        QString title;                                  // From playlist, #EXTINF:<duration>,<title>
        std::chrono::seconds segment_duration = -1s;    // From playlist, e.g. #EXTINF:10.24,
        int64_t first_sequence   = -1;                  // Sequence number of first segment to be recorded
        int64_t sequence_num     = 0;                   // Sequence number of next segment to be read
        int     skipped = 0;                            // Segments skipped, sequence number at or below current

        SegmentContainer new_segments;                  // All segments read from Media Playlist

        QMutexLocker lock(&m_seqLock);
        while (!m_cancel)
        {
            /* Next line */
            line = text.readLine();
            if (line.isNull())
                break;
            LOG(VB_RECORD, (m_debug ? LOG_INFO : LOG_DEBUG),
                LOC + QString("|%1").arg(line));

            if (line.startsWith(QLatin1String("#EXTINF")))
            {
                uint tmp_duration = -1;
                if (!M3U::ParseSegmentInformation(hls->Version(), line,
                                                  tmp_duration,
                                                  title, StreamURL()))
                    return false;
                segment_duration = std::chrono::seconds(tmp_duration);
            }
            else if (line.startsWith(QLatin1String("#EXT-X-TARGETDURATION")))
            {
                int duration = 0;
                if (!M3U::ParseTargetDuration(line, StreamURL(), duration))
                    return false;
                hls->SetTargetDuration(std::chrono::seconds(duration));
            }
            else if (line.startsWith(QLatin1String("#EXT-X-MEDIA-SEQUENCE")))
            {
                if (!M3U::ParseMediaSequence(sequence_num, line, StreamURL()))
                    return false;
                if (first_sequence < 0)
                    first_sequence = sequence_num;
            }
            else if (line.startsWith(QLatin1String("#EXT-X-MEDIA")))
            {
                // Not handled yet
            }
            else if (line.startsWith(QLatin1String("#EXT-X-KEY")))
            {
#ifdef USING_LIBCRYPTO
                QString path;
                QString iv;
                if (!M3U::ParseKey(hls->Version(), line, m_aesMsg,  StreamURL(),
                                   path, iv))
                    return false;
                if (!path.isEmpty())
                    hls->SetKeyPath(path);

                if (!iv.isNull() && !hls->SetAESIV(iv))
                {
                    LOG(VB_RECORD, LOG_ERR, LOC + "invalid IV");
                    return false;
                }
#else
                LOG(VB_RECORD, LOG_ERR, LOC + "#EXT-X-KEY needs libcrypto");
                return false;
#endif
            }
            else if (line.startsWith(QLatin1String("#EXT-X-PROGRAM-DATE-TIME")))
            {
                QDateTime date;
                if (!M3U::ParseProgramDateTime(line, StreamURL(), date))
                    return false;
                // Not handled yet
            }
            else if (line.startsWith(QLatin1String("#EXT-X-ALLOW-CACHE")))
            {
                bool do_cache = false;
                if (!M3U::ParseAllowCache(line, StreamURL(), do_cache))
                    return false;
                hls->SetCache(do_cache);
            }
            else if (line.startsWith(QLatin1String("#EXT-X-DISCONTINUITY-SEQUENCE")))
            {
                int sequence = 0;
                if (!M3U::ParseDiscontinuitySequence(line, StreamURL(), sequence))
                    return false;
                hls->SetDiscontinuitySequence(sequence);
            }
            else if (line.startsWith(QLatin1String("#EXT-X-DISCONTINUITY")))
            {
                if (!M3U::ParseDiscontinuity(line, StreamURL()))
                    return false;
                // Not handled yet
            }
            else if (line.startsWith(QLatin1String("#EXT-X-INDEPENDENT-SEGMENTS")))
            {
                if (!M3U::ParseIndependentSegments(line, StreamURL()))
                    return false;
                // Not handled yet
            }
            else if (line.startsWith(QLatin1String("#EXT-X-VERSION")))
            {
                int version2 = 0;
                if (!M3U::ParseVersion(line, StreamURL(), version2))
                    return false;
                hls->SetVersion(version2);
            }
            else if (line.startsWith(QLatin1String("#EXT-X-ENDLIST")))
            {
                bool is_vod = false;
                if (!M3U::ParseEndList(StreamURL(), is_vod))
                    return false;
                hls->SetLive(!is_vod);
            }
            else if (!line.startsWith(QLatin1String("#")) && !line.isEmpty())
            {
                if (m_curSeq < 0 || sequence_num > m_curSeq)
                {
                    new_segments.push_back
                        (HLSRecSegment(sequence_num, segment_duration, title,
                                       RelativeURI(hls->SegmentBaseUrl(), line)));
                }
                else
                {
                    ++skipped;
                }

                ++sequence_num;
                segment_duration = -1s; /* reset duration */
                title.clear();
            }
        }

        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString(" first_sequence:%1").arg(first_sequence) +
            QString(" sequence_num:%1").arg(sequence_num) +
            QString(" m_curSeq:%1").arg(m_curSeq) +
            QString(" skipped:%1").arg(skipped));

        if (sequence_num < m_curSeq)
        {
            // Sequence has been reset
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Sequence number has been reset from %1 to %2")
                .arg(m_curSeq).arg(first_sequence));
            ResetSequence();
            return false;
        }

        // For near-live skip all segments that are too far in the past.
        if (m_curSeq < 0)
        {
            // Compute number of segments for 30 seconds buffer from live.
            // If the duration is not know keep 3 segments.
            int numseg = new_segments.size();
            numseg = std::min(numseg, 3);
            if (hls->TargetDuration() > 0s)
            {
                numseg = 30s / hls->TargetDuration();
                numseg = std::clamp(numseg, 2, 10);
            }

            // Trim new_segments to leave only the last part
            if (new_segments.size() > numseg)
            {
                int size_before = new_segments.size();
                SegmentContainer::iterator it = new_segments.begin() + (new_segments.size() - numseg);
                new_segments.erase(new_segments.begin(), it);
                LOG(VB_RECORD, LOG_INFO, LOC +
                    QString("Read last %1 segments instead of %2 for near-live")
                        .arg(new_segments.size()).arg(size_before));

                // Adjust first_sequence to first segment to be read
                first_sequence += size_before - new_segments.size();
            }
        }

        SegmentContainer::iterator Inew = new_segments.begin();
        SegmentContainer::iterator Iseg = m_segments.end() - 1;

        // Does this playlist overlap?
        if (!m_segments.empty() && !new_segments.empty())
        {
            if ((*Iseg).Sequence() >= first_sequence &&
                (*Iseg).Sequence() < sequence_num)
            {
                // Find the beginning of the overlap
                while (Iseg != m_segments.begin() &&
                       (*Iseg).Sequence() > first_sequence &&
                       (*Iseg).Sequence() < sequence_num)
                    --Iseg;

                int64_t diff = (*Iseg).Sequence() - (*Inew).Sequence();
                if (diff >= 0 && new_segments.size() > diff)
                {
                    Inew += diff;

                    // Update overlapping segment info
                    for ( ; Iseg != m_segments.end(); ++Iseg, ++Inew)
                    {
                        if (Inew == new_segments.end())
                        {
                            LOG(VB_RECORD, LOG_ERR, LOC +
                                QString("Went off the end with %1 left")
                                .arg(m_segments.end() - Iseg));
                            break;
                        }
                        if ((*Iseg).Sequence() != (*Inew).Sequence())
                        {
                            LOG(VB_RECORD, LOG_ERR, LOC +
                                QString("Sequence non-sequential?  %1 != %2")
                                .arg((*Iseg).Sequence())
                                .arg((*Inew).Sequence()));
                            break;
                        }

                        (*Iseg).m_duration = (*Inew).Duration();
                        (*Iseg).m_title    = (*Inew).Title();
                        (*Iseg).m_url      = (*Inew).Url();
                    }
                }
            }
        }

        for ( ; Inew != new_segments.end(); ++Inew)
            m_segments.push_back(*Inew);

        m_playlistSize = new_segments.size() + skipped;
        int behind     = m_segments.size() - m_playlistSize;
        int max_behind = m_playlistSize / 2;

        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("new_segments.size():%1 ").arg(new_segments.size()) +
            QString("m_playlistSize:%1 ").arg(m_playlistSize) +
            QString("behind:%1 ").arg(behind) +
            QString("max_behind:%1").arg(max_behind));

        if (behind > max_behind)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Not downloading fast enough! "
                        "%1 segments behind, skipping %2 segments. "
                        "playlist size: %3, queued: %4")
                .arg(behind).arg(behind - max_behind)
                .arg(m_playlistSize).arg(m_segments.size()));
            m_workerLock.lock();
            if (m_streamWorker)
                m_streamWorker->CancelCurrentDownload();
            m_workerLock.unlock();

            EnableDebugging();
            Iseg = m_segments.begin() + (behind - max_behind);
            m_segments.erase(m_segments.begin(), Iseg);
            m_bandwidthCheck = (m_bitrateIndex == 0);
        }
        else if (m_debugCnt > 0)
        {
            --m_debugCnt;
        }
        else
        {
            m_debug = false;
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

    m_streamLock.lock();
    if (m_bandwidthCheck /* && !m_segments.empty() */)
    {
        int buffered = PercentBuffered();

        if (buffered < 15)
        {
            // It is taking too long to download the segments
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Falling behind: only %1% buffered").arg(buffered));
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("playlist size %1, queued %2")
                .arg(m_playlistSize).arg(m_segments.size()));
            EnableDebugging();
            DecreaseBitrate(m_curstream->Id());
            m_bandwidthCheck = false;
        }
        else if (buffered > 85)
        {
            // Keeping up easily, raise the bitrate.
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Plenty of bandwidth, downloading %1 of %2")
                .arg(m_playlistSize - m_segments.size())
                .arg(m_playlistSize));
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("playlist size %1, queued %2")
                .arg(m_playlistSize).arg(m_segments.size()));
            IncreaseBitrate(m_curstream->Id());
            m_bandwidthCheck = false;
        }
    }
    m_streamLock.unlock();

    QByteArray buffer;

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    if (!DownloadURL(m_curstream->Url(), &buffer))
        return false;
#else
    QString redir;
    if (!downloader.DownloadURL(m_curstream->M3U8Url(), &buffer, 30s, 0, 0, &redir))
    {
        LOG(VB_GENERAL, LOG_WARNING,
            LOC + "Download failed: " + downloader.ErrorString());
        return false;
    }
#endif

    if (m_segmentBase != redir)
    {
        m_segmentBase = redir;
        m_curstream->SetSegmentBaseUrl(redir);
    }

    if (m_cancel || !ParseM3U8(buffer, m_curstream))
        return false;

    // Signal SegmentWorker that there is work to do...
    m_streamWorker->Wakeup();

    return true;
}

void HLSReader::DecreaseBitrate(int progid)
{
    HLSRecStream *hls = nullptr;
    uint64_t bitrate = m_curstream->Bitrate();
    uint64_t candidate = 0;

    for (auto Istream = m_streams.cbegin(); Istream != m_streams.cend(); ++Istream)
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
    HLSRecStream *hls = nullptr;
    uint64_t bitrate = m_curstream->Bitrate();
    uint64_t candidate = INT_MAX;

    for (auto Istream = m_streams.cbegin(); Istream != m_streams.cend(); ++Istream)
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
        m_bandwidthCheck = (m_bitrateIndex == 0);
        return false;
    }

    HLSRecSegment seg;
    for (;;)
    {
        m_seqLock.lock();
        if (m_cancel || m_segments.empty())
        {
            m_seqLock.unlock();
            break;
        }

        seg = m_segments.front();
        if (m_segments.size() > m_playlistSize)
        {
            LOG(VB_RECORD, (m_debug ? LOG_INFO : LOG_DEBUG), LOC +
                QString("Downloading segment %1 (1 of %2) with %3 behind")
                .arg(seg.Sequence())
                .arg(m_segments.size() + m_playlistSize)
                .arg(m_segments.size() - m_playlistSize));
        }
        else
        {
            LOG(VB_RECORD, (m_debug ? LOG_INFO : LOG_DEBUG), LOC +
                QString("Downloading segment %1 (%2 of %3)")
                .arg(seg.Sequence())
                .arg(m_playlistSize - m_segments.size() + 1)
                .arg(m_playlistSize));
        }
        m_seqLock.unlock();

        m_streamLock.lock();
        HLSRecStream *hls = m_curstream;
        m_streamLock.unlock();
        if (!hls)
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC + "LoadSegment -- no current stream");
            return false;
        }

        long throttle = DownloadSegmentData(downloader,hls,seg,m_playlistSize);

        m_seqLock.lock();
        if (throttle < 0)
        {
            if (m_segments.size() > m_playlistSize)
            {
                SegmentContainer::iterator Iseg = m_segments.begin() +
                                          (m_segments.size() - m_playlistSize);
                m_segments.erase(m_segments.begin(), Iseg);
            }
            m_seqLock.unlock();
            return false;
        }

        m_curSeq = m_segments.front().Sequence();
        m_segments.pop_front();

        m_seqLock.unlock();

        if (m_throttle && throttle == 0)
            throttle = 2;
        else if (throttle > 8)
            throttle = 8;
        if (throttle > 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Throttling -- sleeping %1 secs.")
                .arg(throttle));
            throttle *= 1000;
            m_throttleLock.lock();
            if (m_throttleCond.wait(&m_throttleLock, throttle))
                LOG(VB_RECORD, LOG_INFO, LOC + "Throttle aborted");
            m_throttleLock.unlock();
            LOG(VB_RECORD, LOG_INFO, LOC + "Throttle done");
        }
        else
        {
            usleep(5000);
        }

        if (m_prebufferCnt == 0)
        {
            m_bandwidthCheck = (m_bitrateIndex == 0);
            m_prebufferCnt = 2;
        }
        else
        {
            --m_prebufferCnt;
        }
    }

    LOG(VB_RECORD, LOG_DEBUG, LOC + "LoadSegment -- end");
    return true;
}

uint HLSReader::PercentBuffered(void) const
{
    if (m_playlistSize == 0 || m_segments.size() > m_playlistSize)
        return 0;
    return (static_cast<float>(m_playlistSize - m_segments.size()) /
            static_cast<float>(m_playlistSize)) * 100.0F;
}

int HLSReader::DownloadSegmentData(MythSingleDownload& downloader,
                                   HLSRecStream* hls,
                                   const HLSRecSegment& segment, int playlist_size)
{
    uint64_t bandwidth = hls->AverageBandwidth();

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Downloading seq#%1 av.bandwidth:%2 bitrate %3")
        .arg(segment.Sequence()).arg(bandwidth).arg(hls->Bitrate()));

    /* sanity check - can we download this segment on time? */
    if ((bandwidth > 0) && (hls->Bitrate() > 0) && (segment.Duration().count() > 0))
    {
        uint64_t size = (segment.Duration().count() * hls->Bitrate()); /* bits */
        auto estimated_time = std::chrono::seconds(size / bandwidth);
        if (estimated_time > segment.Duration())
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("downloading of %1 will take %2s, "
                        "which is longer than its playback (%3s) at %4KiB/s")
                .arg(segment.Sequence())
                .arg(estimated_time.count())
                .arg(segment.Duration().count())
                .arg(bandwidth / 8192));
        }
    }

    QByteArray buffer;
    auto start = nowAsDuration<std::chrono::milliseconds>();

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
                                   // and can only handle six download at a time
    if (!HLSReader::DownloadURL(segment.Url(), &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("%1 failed").arg(segment.Sequence()));
        if (estimated_time * 2 < segment.Duration())
        {
            if (!HLSReader::DownloadURL(segment.Url(), &buffer))
            {
                LOG(VB_RECORD, LOG_ERR, LOC + QString("%1 failed")
                    .arg(segment.Sequence()));
                return -1;
            }
        }
        else
            return 0;
    }
#else
    if (!downloader.DownloadURL(segment.Url(), &buffer))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("%1 failed: %2")
            .arg(segment.Sequence()).arg(downloader.ErrorString()));
        return -1;
    }
#endif

    auto downloadduration = nowAsDuration<std::chrono::milliseconds>() - start;

#ifdef USING_LIBCRYPTO
    /* If the segment is encrypted, decode it */
    if (segment.HasKeyPath())
    {
        if (!hls->DecodeData(downloader, hls->IVLoaded() ? hls->AESIV() : QByteArray(),
                             segment.KeyPath(),
                             buffer, segment.Sequence()))
            return 0;
    }
#endif

    int64_t segment_len = buffer.size();

    m_bufLock.lock();
    if (m_buffer.size() > segment_len * playlist_size)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("streambuffer is not reading fast enough. "
                    "buffer size %1").arg(m_buffer.size()));
        EnableDebugging();
        if (++m_slowCnt > 15)
        {
            m_slowCnt = 15;
            m_fatal = true;
            return -1;
        }
    }
    else if (m_slowCnt > 0)
    {
        --m_slowCnt;
    }

    if (m_buffer.size() >= segment_len * playlist_size * 2)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("streambuffer is not reading fast enough. "
                    "buffer size %1.  Dropping %2 bytes")
            .arg(m_buffer.size()).arg(segment_len));
        m_buffer.remove(0, segment_len);
    }

    m_buffer += buffer;
    m_bufLock.unlock();

    if (hls->Bitrate() == 0 && segment.Duration() > 0s)
    {
        /* Try to estimate the bandwidth for this stream */
        hls->SetBitrate((uint64_t)(((double)segment_len * 8) /
                                   ((double)segment.Duration().count())));
    }

    if (downloadduration < 1ms)
        downloadduration = 1ms;

    /* bits/sec */
    bandwidth = 8ULL * 1000 * segment_len / downloadduration.count();
    hls->AverageBandwidth(bandwidth);
    if (segment.Duration() > 0s)
    {
        hls->SetCurrentByteRate(segment_len/segment.Duration().count());
    }

    LOG(VB_RECORD, (m_debug ? LOG_INFO : LOG_DEBUG), LOC +
        QString("%1 took %2ms for %3 bytes: bandwidth:%4KiB/s byterate:%5KiB/s")
        .arg(segment.Sequence())
        .arg(downloadduration.count())
        .arg(segment_len)
        .arg(bandwidth / 8192.0)
        .arg(hls->CurrentByteRate() / 1024.0));

    return m_slowCnt;
}

void HLSReader::PlaylistGood(void)
{
    QMutexLocker lock(&m_streamLock);
    if (m_curstream)
        m_curstream->Good();
}

void HLSReader::PlaylistRetrying(void)
{
    QMutexLocker lock(&m_streamLock);
    if (m_curstream)
        m_curstream->Retrying();
}

int  HLSReader::PlaylistRetryCount(void) const
{
    QMutexLocker lock(&m_streamLock);
    if (m_curstream)
        return m_curstream->RetryCount();
    return 0;
}

void HLSReader::EnableDebugging(void)
{
    m_debug = true;
    m_debugCnt = 5;
    LOG(VB_RECORD, LOG_INFO, LOC + "Debugging enabled");
}
