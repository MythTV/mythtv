#include "mythdate.h"

#include <QtGlobal>
#include <QCoreApplication>

#include "mythcorecontext.h"

namespace MythDate
{

/** \fn toString(const QDateTime&,uint)
 *  \brief Returns a formatted QString based on the supplied QDateTime
 *
 *  \param raw_dt    The QDateTime object to use
 *  \param format   The format of the string to return
 */
QString toString(const QDateTime &raw_dt, uint format)
{
    QString result;

    if (!raw_dt.isValid())
        return result;

    // if no format is set default to UTC for ISO/file/DB dates.
    if (!((format & kOverrideUTC) || (format & kOverrideLocal)))
    {
        format |= ((ISODate|kFilename|kDatabase) & format) ?
            kOverrideUTC : kOverrideLocal;
    }

    QDateTime datetime =
        (format & kOverrideUTC) ? raw_dt.toUTC() : raw_dt.toLocalTime();

    if (format & kDatabase)
        return datetime.toString("yyyy-MM-dd hh:mm:ss");

    if (format & MythDate::ISODate)
        return datetime.toString(Qt::ISODate);

    if (format & MythDate::kRFC822) // RFC 822 - RFC 7231 Sect 7.1.1.1 - HTTP Date
        return datetime.toUTC().toString("ddd, dd MMM yyyy hh:mm:ss").append(" GMT");

    if (format & kFilename)
        return datetime.toString("yyyyMMddhhmmss");

    if (format & kScreenShotFilename)
        return datetime.toString("yyyy-MM-ddThh-mm-ss.zzz");

    if (format & kDateEither)
        result += toString(datetime.date(), format);

    if (format & kTime)
    {
        if (!result.isEmpty())
            result.append(", ");

        QString timeformat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");
        result += datetime.time().toString(timeformat);
    }

    return result;
}

/** \brief Returns a formatted QString based on the supplied QDate
 *
 *  \param date     The QDate object to use
 *  \param format   The format of the string to return
 */
QString toString(const QDate date, uint format)
{
    QString result;

    if (!date.isValid())
        return result;

    if (format & kDateEither)
    {
        QString stringformat;
        if (format & kDateShort)
            stringformat = gCoreContext->GetSetting("ShortDateFormat", "ddd d");
        else
            stringformat = gCoreContext->GetSetting("DateFormat", "ddd d MMMM");

        if (format & kAddYear)
        {
            if (!stringformat.contains("yy")) // Matches both 2 or 4 digit year
                stringformat.append(" yyyy");
        }

        if (format & kAutoYear)
        {
            if (!stringformat.contains("yy") // Matches both 2 or 4 digit year
                && date.year() != QDateTime::currentDateTime().date().year())
                stringformat.append(" yyyy");
        }

        if (format & ~kDateShort)
        {
            QDate now = current().toLocalTime().date();
            if ((format & kSimplify) && (now == date))
                result = QCoreApplication::translate("(Common)", "Today");
            else if (((format & kSimplify) != 0U) && (now.addDays(-1) == date))
                result = QCoreApplication::translate("(Common)", "Yesterday");
            else if (((format & kSimplify) != 0U) && (now.addDays(1) == date))
                result = QCoreApplication::translate("(Common)", "Tomorrow");
        }

        if (result.isEmpty())
            result = gCoreContext->GetQLocale().toString(date, stringformat);
    }

    return result;
}

} // namespace MythDate
