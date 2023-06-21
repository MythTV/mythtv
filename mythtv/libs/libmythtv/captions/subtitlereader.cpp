#include "libmythbase/mythlogging.h"
#include "captions/subtitlereader.h"

// If the count of subtitle buffers is greater than this, force a clear
static const int MAX_BUFFERS_BEFORE_CLEAR = 175;  // 125 too low for karaoke
static const int MAX_EXT_BUFFERS_BEFORE_CLEAR = 500;  // 2 hr movie, titles every 15 s

SubtitleReader::SubtitleReader(MythPlayer *parent)
  : m_parent(parent)
{
    connect(&m_textSubtitles, &TextSubtitles::TextSubtitlesUpdated,
            this, &SubtitleReader::TextSubtitlesUpdated);
}

SubtitleReader::~SubtitleReader()
{
    ClearAVSubtitles(true);
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

bool SubtitleReader::AddAVSubtitle(AVSubtitle &subtitle,
                                   bool fix_position,
                                   bool allow_forced)
{
    bool enableforced = false;
    bool forced = false;
    for (unsigned i = 0; i < subtitle.num_rects; i++)
    {
        forced = forced || static_cast<bool>(subtitle.rects[i]->flags & AV_SUBTITLE_FLAG_FORCED);
    }

    if (!m_avSubtitlesEnabled)
    {
        if (!forced)
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

    bool clearsubs = false;
    m_avSubtitles.m_lock.lock();
    m_avSubtitles.m_fixPosition = fix_position;
    m_avSubtitles.m_buffers.push_back(subtitle);
    // in case forced subtitles aren't displayed, avoid leaking by
    // manually clearing the subtitles
    size_t max_buffers = m_externalParser
        ? MAX_EXT_BUFFERS_BEFORE_CLEAR
        : MAX_BUFFERS_BEFORE_CLEAR;
    if (m_avSubtitles.m_buffers.size() > max_buffers)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("SubtitleReader: >%1 AVSubtitles queued - clearing.")
                .arg(max_buffers));
        clearsubs = true;
    }
    m_avSubtitles.m_lock.unlock();

    if (clearsubs)
        ClearAVSubtitles();

    return enableforced;
}

void SubtitleReader::ClearAVSubtitles(bool force)
{
    // Don't flush subtitles loaded from an external file.  They won't
    // get repopulated as the stream continues to play.
    if (m_externalParser && !force)
        return;

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
