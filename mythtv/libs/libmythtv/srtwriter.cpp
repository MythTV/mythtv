#include "srtwriter.h"

// SRTWriter implementation

/**
 * Adds next subtitle.
 */
void SRTWriter::AddSubtitle(const OneSubtitle &sub, int number)
{
    m_outStream << number << endl;

    m_outStream << FormatTime(sub.start_time) << " --> ";
    m_outStream << FormatTime(sub.start_time + sub.length) << endl;

    if (!sub.text.isEmpty())
    {
        QStringList::const_iterator it = sub.text.begin();
        for (; it != sub.text.end(); ++it)
            m_outStream << *it << endl;
        m_outStream << endl;
    }
}

/**
 * Formats time to format appropriate to SubRip file.
 */
QString SRTWriter::FormatTime(uint64_t time_in_msec)
{
    uint64_t msec = time_in_msec % 1000;
    time_in_msec /= 1000;

    uint64_t ss = time_in_msec % 60;
    time_in_msec /= 60;

    uint64_t mm = time_in_msec % 60;
    time_in_msec /= 60;

    uint64_t hh = time_in_msec;

    return QString("%1:%2:%3,%4")
        .arg(hh,2,10,QChar('0'))
        .arg(mm,2,10,QChar('0'))
        .arg(ss,2,10,QChar('0'))
        .arg(msec,3,10,QChar('0'));
}
