#include "captions/srtwriter.h"

#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
  #define QT_ENDL endl
#else
  #define QT_ENDL Qt::endl
#endif

// SRTWriter implementation

/**
 * Adds next subtitle.
 */
void SRTWriter::AddSubtitle(const OneSubtitle &sub, int number)
{
    m_outStream << number << QT_ENDL;

    m_outStream << FormatTime(sub.m_startTime) << " --> ";
    m_outStream << FormatTime(sub.m_startTime + sub.m_length) << QT_ENDL;

    if (!sub.m_text.isEmpty())
    {
        for (const auto & text : qAsConst(sub.m_text))
            m_outStream << text << QT_ENDL;
        m_outStream << QT_ENDL;
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
