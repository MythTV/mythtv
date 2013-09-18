#include <QtGlobal>
#include <QCoreApplication>

#include "mythcorecontext.h"
#include "mythdate.h"

namespace MythDate
{

QDateTime current(bool stripped)
{
#if QT_VERSION >= QT_VERSION_CHECK(4,7,0)
    QDateTime rettime = QDateTime::currentDateTimeUtc();
#else
    QDateTime rettime = QDateTime::currentDateTime().toUTC();
#endif
    if (stripped)
        rettime = rettime.addMSecs(-rettime.time().msec());
    return rettime;
}

QString current_iso_string(bool stripped)
{
    return MythDate::current(stripped).toString(Qt::ISODate);
}

QDateTime as_utc(const QDateTime &old_dt)
{
    QDateTime dt(old_dt);
    dt.setTimeSpec(Qt::UTC);
    return dt;
}

QDateTime fromString(const QString &dtstr)
{
    QDateTime dt;
    if (dtstr.isEmpty())
        return as_utc(dt);

    if (!dtstr.contains("-") && dtstr.length() == 14)
    {
        // must be in yyyyMMddhhmmss format
        dt = QDateTime::fromString(dtstr, "yyyyMMddhhmmss");
    }
    else
    {
        dt = QDateTime::fromString(dtstr, Qt::ISODate);
    }

    return as_utc(dt);
}

MBASE_PUBLIC QDateTime fromString(const QString &str, const QString &format)
{
    QDateTime dt = QDateTime::fromString(str, format);
    dt.setTimeSpec(Qt::UTC);
    return dt;
}

MBASE_PUBLIC QDateTime fromTime_t(uint seconds)
{
    QDateTime dt = QDateTime::fromTime_t(seconds);
    return dt.toUTC();
}

/** \fn toString(const QDateTime&,uint)
 *  \brief Returns a formatted QString based on the supplied QDateTime
 *
 *  \param datetime The QDateTime object to use
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

    if (format & kFilename)
        return datetime.toString("yyyyMMddhhmmss");

    if (format & kScreenShotFilename)
        return datetime.toString("yyyy-MM-ddThh-mm-ss.zzz");

    if (format & (kDateFull | kDateShort))
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
QString toString(const QDate &date, uint format)
{
    QString result;

    if (!date.isValid())
        return result;

    if (format & (kDateFull | kDateShort))
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

        if (format & ~kDateShort)
        {
            QDate now = current().toLocalTime().date();
            if ((format & kSimplify) && (now == date))
                result = QCoreApplication::translate("(Common)", "Today");
            else if ((format & kSimplify) && (now.addDays(-1) == date))
                result = QCoreApplication::translate("(Common)", "Yesterday");
            else if ((format & kSimplify) && (now.addDays(1) == date))
                result = QCoreApplication::translate("(Common)", "Tomorrow");
        }

        if (result.isEmpty())
            result = gCoreContext->GetQLocale().toString(date, stringformat);
    }

    return result;
}

/** \brief Returns the total number of seconds since midnight of the supplied QTime
 *
 *  \param date     The QDate object to use
 *  \param format   The format of the string to return
 */
int toSeconds( const QTime &time )
{
    if (!time.isValid())
        return 0;

    int nSecs = time.hour() * 3600;
    nSecs += (time.minute() * 60 );
    nSecs += time.second();

    return nSecs;
}

}; // namespace MythDate
