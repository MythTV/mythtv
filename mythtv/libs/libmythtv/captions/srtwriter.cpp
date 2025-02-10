#include "captions/srtwriter.h"

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif
#include <QTime>

#include "libmythbase/mythlogging.h"

// SRTWriter implementation

SRTWriter::SRTWriter(const QString &fileName) :
    m_outFile(fileName), m_outStream(&m_outFile)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_outStream.setCodec("UTF-8");
#else
    m_outStream.setEncoding(QStringConverter::Utf8);
#endif
    if (!m_outFile.open(QFile::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to create '%1'").arg(fileName));
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Created '%1'").arg(fileName));
    }
}

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
