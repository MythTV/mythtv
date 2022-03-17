#include "libmythbase/mythlogging.h"
#include "captions/subtitlereader.h"

// If the count of subtitle buffers is greater than this, force a clear
static const int MAX_BUFFERS_BEFORE_CLEAR = 175;  // 125 too low for karaoke

SubtitleReader::SubtitleReader()
{
    connect(&m_textSubtitles, &TextSubtitles::TextSubtitlesUpdated,
            this, &SubtitleReader::TextSubtitlesUpdated);
}

SubtitleReader::~SubtitleReader()
{
    ClearAVSubtitles();
    m_textSubtitles.Clear();
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
    if (!m_avSubtitlesEnabled && !subtitle.forced)
    {
        FreeAVSubtitle(subtitle);
        return enableforced;
    }

    if (!m_avSubtitlesEnabled && subtitle.forced)
    {
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
    if (m_avSubtitles.m_buffers.size() > MAX_BUFFERS_BEFORE_CLEAR)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("SubtitleReader: >%1 AVSubtitles queued - clearing.")
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
    m_textSubtitles.Clear();
    m_textSubtitles.SetInProgress(isInProgress);
    if (!subtitleFileName.isEmpty())
        TextSubtitleParser::LoadSubtitles(subtitleFileName, m_textSubtitles, false);
}

bool SubtitleReader::HasTextSubtitles(void)
{
    return m_textSubtitles.GetSubtitleCount() >= 0;
}

QStringList SubtitleReader::GetRawTextSubtitles(std::chrono::milliseconds &duration)
{
    QMutexLocker lock(&m_rawTextSubtitles.m_lock);
    if (m_rawTextSubtitles.m_buffers.empty())
        return QStringList();

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
