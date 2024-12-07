#include "libmythbase/mythlogging.h"
#include "captions/subtitlereader.h"

#ifdef DEBUG_SUBTITLES
QString toString(const AVSubtitle& sub)
{
    auto start = QTime(0,0).addMSecs(sub.start_display_time);
    auto end = QTime(0,0).addMSecs(sub.end_display_time);
    return QString("%1-%2 (%3-%4)")
        .arg(start.toString("HH:mm:ss.zzz"),
             end.toString("HH:mm:ss.zzz"))
        .arg(sub.start_display_time)
        .arg(sub.end_display_time);
}
#endif

// If the count of subtitle buffers is greater than this, force a clear
static const int MAX_BUFFERS_BEFORE_CLEAR = 175;  // 125 too low for karaoke

SubtitleReader::SubtitleReader(MythPlayer *parent)
  : m_parent(parent)
{
    connect(&m_textSubtitles, &TextSubtitles::TextSubtitlesUpdated,
            this, &SubtitleReader::TextSubtitlesUpdated);
}

SubtitleReader::~SubtitleReader()
{
    ClearAVSubtitles();
    ClearRawTextSubtitles();
}

void SubtitleReader::EnableAVSubtitles(bool enable)
{
    m_avSubtitlesEnabled = enable;
}

void SubtitleReader::EnableTextSubtitles(bool enable)
{
    m_textSubtitlesEnabled = enable;
}

void SubtitleReader::EnableRawTextSubtitles(bool enable)
{
    m_rawTextSubtitlesEnabled = enable;
}

void SubtitleReader::SeekFrame(int64_t ts, int flags)
{
    m_avSubtitles.m_needSync = true;
    if (m_externalParser)
    {
        ClearAVSubtitles();
        m_externalParser->SeekFrame(ts, flags);
    }
}

bool SubtitleReader::AddAVSubtitle(AVSubtitle &subtitle,
                                   bool fix_position,
                                   bool is_selected_forced_track,
                                   bool allow_forced,
                                   bool isExternal)
{
    bool enableforced = false;
    bool forced = false;

    if (m_avSubtitlesEnabled && is_selected_forced_track)
    {
        FreeAVSubtitle(subtitle);
        return enableforced;
    }

    for (unsigned i = 0; i < subtitle.num_rects; i++)
    {
        forced = forced || static_cast<bool>(subtitle.rects[i]->flags & AV_SUBTITLE_FLAG_FORCED);
    }

    if (!m_avSubtitlesEnabled && !isExternal)
    {
        if (!(is_selected_forced_track || forced))
        {
            FreeAVSubtitle(subtitle);
            return enableforced;
        }

        if (!allow_forced)
        {
            LOG(VB_PLAYBACK, LOG_INFO,
                "SubtitleReader: Ignoring forced AV subtitle.");
            FreeAVSubtitle(subtitle);
            return enableforced;
        }

        LOG(VB_PLAYBACK, LOG_INFO,
            "SubtitleReader: Allowing forced AV subtitle.");
        enableforced = true;
    }

    if ((m_textSubtitlesEnabled && !isExternal) ||
        (m_avSubtitlesEnabled && isExternal))
    {
        FreeAVSubtitle(subtitle);
        return enableforced;
    }

    // Sanity check to prevent seg fault
    if (subtitle.num_rects > 1000)
    {
        FreeAVSubtitle(subtitle);
        return enableforced;
    }

#ifdef DEBUG_SUBTITLES
    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
    {
        LOG(VB_PLAYBACK, LOG_DEBUG,
            QString("AddAVSubtitle adding sub %1").arg(toString(subtitle)));
    }
#endif

    bool clearsubs = false;
    m_avSubtitles.m_lock.lock();
    m_avSubtitles.m_fixPosition = fix_position;
    m_avSubtitles.m_buffers.push_back(subtitle);
    // in case forced subtitles aren't displayed, avoid leaking by
    // manually clearing the subtitles
    if (!m_externalParser &&
        (m_avSubtitles.m_buffers.size() > MAX_BUFFERS_BEFORE_CLEAR))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("SubtitleReader: %1 AVSubtitles queued (more than %2) - clearing.")
                .arg(m_avSubtitles.m_buffers.size())
                .arg(MAX_BUFFERS_BEFORE_CLEAR));
        clearsubs = true;
    }
    m_avSubtitles.m_lock.unlock();

    if (clearsubs)
        ClearAVSubtitles();

    return enableforced;
}

void SubtitleReader::ClearAVSubtitles(void)
{
    m_avSubtitles.m_lock.lock();
    while (!m_avSubtitles.m_buffers.empty())
    {
        FreeAVSubtitle(m_avSubtitles.m_buffers.front());
        m_avSubtitles.m_buffers.pop_front();
    }
    m_avSubtitles.m_lock.unlock();
}

void SubtitleReader::FreeAVSubtitle(AVSubtitle &subtitle)
{
    avsubtitle_free(&subtitle);
}

void SubtitleReader::LoadExternalSubtitles(const QString &subtitleFileName,
                                           bool isInProgress)
{
    m_textSubtitles.SetInProgress(isInProgress);
    if (!subtitleFileName.isEmpty())
    {
        m_externalParser = new TextSubtitleParser(this, subtitleFileName, &m_textSubtitles);
        m_externalParser->LoadSubtitles(false);
    }
}

int SubtitleReader::ReadNextSubtitle(void)
{
    m_avSubtitles.m_needSync = false;
    if (m_externalParser)
        return m_externalParser->ReadNextSubtitle();
    return -1;
}

bool SubtitleReader::HasTextSubtitles(void)
{
    return m_textSubtitles.GetHasSubtitles();
}

QStringList SubtitleReader::GetRawTextSubtitles(std::chrono::milliseconds &duration)
{
    QMutexLocker lock(&m_rawTextSubtitles.m_lock);
    if (m_rawTextSubtitles.m_buffers.empty())
        return {};

    duration = m_rawTextSubtitles.m_duration;
    QStringList result = m_rawTextSubtitles.m_buffers;
    m_rawTextSubtitles.m_buffers.clear();
    return result;
}

void SubtitleReader::AddRawTextSubtitle(const QStringList& list, std::chrono::milliseconds duration)
{
    if (!m_rawTextSubtitlesEnabled || list.empty())
        return;

    QMutexLocker lock(&m_rawTextSubtitles.m_lock);
    m_rawTextSubtitles.m_buffers.clear();
    m_rawTextSubtitles.m_buffers = list;
    m_rawTextSubtitles.m_duration = duration;
}

void SubtitleReader::ClearRawTextSubtitles(void)
{
    QMutexLocker lock(&m_rawTextSubtitles.m_lock);
    m_rawTextSubtitles.m_buffers.clear();
}
