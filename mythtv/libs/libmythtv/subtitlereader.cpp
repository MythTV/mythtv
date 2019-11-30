#include "mythlogging.h"
#include "subtitlereader.h"

SubtitleReader::~SubtitleReader()
{
    ClearAVSubtitles();
    m_TextSubtitles.Clear();
    ClearRawTextSubtitles();
}

void SubtitleReader::EnableAVSubtitles(bool enable)
{
    m_AVSubtitlesEnabled = enable;
}

void SubtitleReader::EnableTextSubtitles(bool enable)
{
    m_TextSubtitlesEnabled = enable;
}

void SubtitleReader::EnableRawTextSubtitles(bool enable)
{
    m_RawTextSubtitlesEnabled = enable;
}

bool SubtitleReader::AddAVSubtitle(AVSubtitle &subtitle,
                                   bool fix_position,
                                   bool allow_forced)
{
    bool enableforced = false;
    if (!m_AVSubtitlesEnabled && !subtitle.forced)
    {
        FreeAVSubtitle(subtitle);
        return enableforced;
    }

    if (!m_AVSubtitlesEnabled && subtitle.forced)
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
    m_AVSubtitles.m_lock.lock();
    m_AVSubtitles.m_fixPosition = fix_position;
    m_AVSubtitles.m_buffers.push_back(subtitle);
    // in case forced subtitles aren't displayed, avoid leaking by
    // manually clearing the subtitles
    if (m_AVSubtitles.m_buffers.size() > 40)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "SubtitleReader: >40 AVSubtitles queued - clearing.");
        clearsubs = true;
    }
    m_AVSubtitles.m_lock.unlock();

    if (clearsubs)
        ClearAVSubtitles();

    return enableforced;
}

void SubtitleReader::ClearAVSubtitles(void)
{
    m_AVSubtitles.m_lock.lock();
    while (!m_AVSubtitles.m_buffers.empty())
    {
        FreeAVSubtitle(m_AVSubtitles.m_buffers.front());
        m_AVSubtitles.m_buffers.pop_front();
    }
    m_AVSubtitles.m_lock.unlock();
}

void SubtitleReader::FreeAVSubtitle(AVSubtitle &subtitle)
{
    avsubtitle_free(&subtitle);
}

void SubtitleReader::LoadExternalSubtitles(const QString &subtitleFileName,
                                           bool isInProgress)
{
    m_TextSubtitles.Clear();
    m_TextSubtitles.SetInProgress(isInProgress);
    TextSubtitleParser::LoadSubtitles(subtitleFileName, m_TextSubtitles,
                                      false);
}

bool SubtitleReader::HasTextSubtitles(void)
{
    return m_TextSubtitles.GetSubtitleCount() >= 0;
}

QStringList SubtitleReader::GetRawTextSubtitles(uint64_t &duration)
{
    QMutexLocker lock(&m_RawTextSubtitles.m_lock);
    if (m_RawTextSubtitles.m_buffers.empty())
        return QStringList();

    duration = m_RawTextSubtitles.m_duration;
    QStringList result = m_RawTextSubtitles.m_buffers;
    m_RawTextSubtitles.m_buffers.clear();
    return result;
}

void SubtitleReader::AddRawTextSubtitle(const QStringList& list, uint64_t duration)
{
    if (!m_RawTextSubtitlesEnabled || list.empty())
        return;

    QMutexLocker lock(&m_RawTextSubtitles.m_lock);
    m_RawTextSubtitles.m_buffers.clear();
    m_RawTextSubtitles.m_buffers = list;
    m_RawTextSubtitles.m_duration = duration;
}

void SubtitleReader::ClearRawTextSubtitles(void)
{
    QMutexLocker lock(&m_RawTextSubtitles.m_lock);
    m_RawTextSubtitles.m_buffers.clear();
}
