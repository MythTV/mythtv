#include "subtitlereader.h"

SubtitleReader::SubtitleReader()
  : m_AVSubtitlesEnabled(false), m_TextSubtitlesEnabled(false)
{
}

SubtitleReader::~SubtitleReader()
{
    ClearAVSubtitles();
    m_TextSubtitles.Clear();
}

void SubtitleReader::EnableAVSubtitles(bool enable)
{
    m_AVSubtitlesEnabled = enable;
}

void SubtitleReader::EnableTextSubtitles(bool enable)
{
    m_TextSubtitlesEnabled = enable;
}

void SubtitleReader::AddAVSubtitle(const AVSubtitle &subtitle)
{
    if (!m_AVSubtitlesEnabled)
    {
        FreeAVSubtitle(subtitle);
        return;
    }
    m_AVSubtitles.lock.lock();
    m_AVSubtitles.buffers.push_back(subtitle);
    m_AVSubtitles.lock.unlock();
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

bool SubtitleReader::LoadExternalSubtitles(const QString &subtitleFileName)
{
    m_TextSubtitles.Clear();
    return TextSubtitleParser::LoadSubtitles(subtitleFileName, m_TextSubtitles);
}

bool SubtitleReader::HasTextSubtitles(void)
{
    return m_TextSubtitles.GetSubtitleCount() > 0;
}
