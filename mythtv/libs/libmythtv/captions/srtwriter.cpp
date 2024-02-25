#include "captions/srtwriter.h"

// SRTWriter implementation

/**
 * Adds next subtitle.
 */
void SRTWriter::AddSubtitle(const OneSubtitle &sub, int number)
{
    m_outStream << number << Qt::endl;

    m_outStream << FormatTime(sub.m_startTime) << " --> ";
    m_outStream << FormatTime(sub.m_startTime + sub.m_length) << Qt::endl;

    if (!sub.m_text.isEmpty())
    {
        for (const auto & text : std::as_const(sub.m_text))
            m_outStream << text << Qt::endl;
        m_outStream << Qt::endl;
    }
}

/**
 * Formats time to format appropriate to SubRip file.
 */
QString SRTWriter::FormatTime(std::chrono::milliseconds time_in_msec)
{
    QTime time = QTime::fromMSecsSinceStartOfDay(time_in_msec.count());
    return time.toString("HH:mm:ss,zzz");
}
