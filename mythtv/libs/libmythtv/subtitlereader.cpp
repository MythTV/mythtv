#include "mythlogging.h"
#include "subtitlereader.h"

SubtitleReader::SubtitleReader()
  : m_AVSubtitlesEnabled(false), m_TextSubtitlesEnabled(false),
    m_RawTextSubtitlesEnabled(false)
{
}

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

bool SubtitleReader::AddAVSubtitle(const AVSubtitle &subtitle,
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
    m_AVSubtitles.lock.lock();
    m_AVSubtitles.fixPosition = fix_position;
    m_AVSubtitles.buffers.push_back(subtitle);
    // in case forced subtitles aren't displayed, avoid leaking by
    // manually clearing the subtitles
    if (m_AVSubtitles.buffers.size() > 20)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "SubtitleReader: >20 AVSubtitles queued - clearing.");
        clearsubs = true;
    }
    m_AVSubtitles.lock.unlock();

    if (clearsubs)
        ClearAVSubtitles();

    return enableforced;
}

void SubtitleReader::ClearAVSubtitles(void)
{
    m_AVSubtitles.lock.lock();
    while (!m_AVSubtitles.buffers.empty())
    {
        FreeAVSubtitle(m_AVSubtitles.buffers.front());
        m_AVSubtitles.buffers.pop_front();
    }
    m_AVSubtitles.lock.unlock();
}

void SubtitleReader::FreeAVSubtitle(const AVSubtitle &subtitle)
{
    for (std::size_t i = 0; i < subtitle.num_rects; ++i)
    {
         AVSubtitleRect* rect = subtitle.rects[i];
         av_free(rect->pict.data[0]);
         av_free(rect->pict.data[1]);
    }
    if (subtitle.num_rects > 0)
        av_free(subtitle.rects);
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
    QMutexLocker lock(&m_RawTextSubtitles.lock);
    if (m_RawTextSubtitles.buffers.empty())
        return QStringList();

    duration = m_RawTextSubtitles.duration;
    QStringList result = m_RawTextSubtitles.buffers;
    result.detach();
    m_RawTextSubtitles.buffers.clear();
    return result;
}

void SubtitleReader::AddRawTextSubtitle(QStringList list, uint64_t duration)
{
    if (!m_RawTextSubtitlesEnabled || list.empty())
        return;

    QMutexLocker lock(&m_RawTextSubtitles.lock);
    m_RawTextSubtitles.buffers.clear();
    m_RawTextSubtitles.buffers = list;
    m_RawTextSubtitles.duration = duration;
}

void SubtitleReader::ClearRawTextSubtitles(void)
{
    QMutexLocker lock(&m_RawTextSubtitles.lock);
    m_RawTextSubtitles.buffers.clear();
}
