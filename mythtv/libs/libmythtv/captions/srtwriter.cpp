#include "captions/srtwriter.h"

// SRTWriter implementation

/**
 * Adds next subtitle.
 */
void SRTWriter::AddSubtitle(const OneSubtitle &sub, int number)
{
    m_outStream << number << endl;

    m_outStream << FormatTime(sub.m_startTime) << " --> ";
    m_outStream << FormatTime(sub.m_startTime + sub.m_length) << endl;

    if (!sub.m_text.isEmpty())
    {
        for (const auto & text : qAsConst(sub.m_text))
            m_outStream << text << endl;
        m_outStream << endl;
    }
}

/**
 * Formats time to format appropriate to SubRip file.
 */
QString SRTWriter::FormatTime(uint64_t time_in_msec)
{
    QTime time = QTime::fromMSecsSinceStartOfDay(time_in_msec);
    return time.toString("HH:mm:ss,zzz");
}
