#include <array>

#include <QtGlobal>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QTimeZone>

#include "mythcorecontext.h"
#include "mythdate.h"
#include "stringutil.h"

namespace MythDate
{

QDateTime current(bool stripped)
{
    QDateTime rettime = QDateTime::currentDateTimeUtc();
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
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    dt.setTimeSpec(Qt::UTC);
#else
    dt.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    dt.setTimeSpec(Qt::UTC);
#else
    dt.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
    return dt;
}

/**
 *  This function takes the number of seconds since the start of the
 *  epoch and returns a QDateTime with the equivalent value.
 *
 *  Note: This function returns a QDateTime set to UTC, whereas the
 *  QDateTime::fromSecsSinceEpoch function returns a value set to
 *  localtime.
 *
 *  \param seconds  The number of seconds since the start of the epoch
 *                  at Jan 1 1970 at 00:00:00.
 *  \return A QDateTime.
 */
MBASE_PUBLIC QDateTime fromSecsSinceEpoch(int64_t seconds)
{
    QDateTime dt = QDateTime::fromSecsSinceEpoch(seconds);
    return dt.toUTC();
}

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

/** \brief Returns the total number of seconds since midnight of the supplied QTime
 *
 *  \param time     The QTime object to use
 */
std::chrono::seconds toSeconds(QTime time)
{
    if (!time.isValid())
        return 0s;

    std::chrono::seconds nSecs = std::chrono::hours(time.hour());
    nSecs += std::chrono::minutes(time.minute());
    nSecs += std::chrono::seconds(time.second());

    return nSecs;
}

std::chrono::milliseconds currentMSecsSinceEpochAsDuration(void)
{
    return std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch());
};

std::chrono::seconds secsInPast (const QDateTime& past)
{
    return std::chrono::seconds(past.secsTo(MythDate::current()));
}

std::chrono::seconds secsInFuture (const QDateTime& future)
{
    return std::chrono::seconds(MythDate::current().secsTo(future));
}

/**
 * \brief Format a milliseconds time value
 *
 * Convert a millisecond time value into a textual representation of
 * the value. QTime can't handle overflow of any of the fields, so the
 * formatting needs to be done manually.  Think a music playlist of
 * more than 24 hours, or a single song of more than 60 minutes
 * (e.g. a podcast or something like that).
 *
 * \param msecs The time value in milliseconds. Since the type of this
 *     field is std::chrono::duration, any duration of a larger
 *     interval can be passed to this function and the compiler will
 *     convert it to milliseconds.
 *
 * \param fmt A formatting string specifying how to output the time.
 *     Valid formatting characters are <tt>"Hmsz"</tt> for hours, minutes, seconds,
 *     and milliseconds, respectively.  Consecutive runs of these characters will
 *     be replaced by at least as many characters as the run length, zero padding
 *     if necessary.
 */
QString formatTime(std::chrono::milliseconds msecs, QString fmt)
{
    static const QRegularExpression hRe("H+");
    static const QRegularExpression mRe("m+");
    static const QRegularExpression sRe("s+");
    static const QRegularExpression zRe("z+");

    bool negativeTime = msecs < 0ms;
    msecs = std::chrono::milliseconds(std::abs(msecs.count()));

    QRegularExpressionMatch match = hRe.match(fmt);
    if (match.hasMatch())
    {
        int width = match.capturedLength();
        QString text = StringUtil::intToPaddedString(msecs / 1h, width);
        fmt.replace(match.capturedStart(), width, text);
        msecs = msecs % 1h;
    }

    match = mRe.match(fmt);
    if (match.hasMatch())
    {
        int width = match.capturedLength();
        QString text = StringUtil::intToPaddedString(msecs / 1min, width);
        fmt.replace(match.capturedStart(), width, text);
        msecs = msecs % 1min;
    }

    match = sRe.match(fmt);
    if (match.hasMatch())
    {
        int width = match.capturedLength();
        QString text = StringUtil::intToPaddedString(msecs / 1s, width);
        fmt.replace(match.capturedStart(), width, text);
    }

    match = zRe.match(fmt);
    if (match.hasMatch())
    {
        static constexpr std::array<int,4> divisor = {1000, 100, 10, 1};
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        int width = std::min(3, match.capturedLength());
#else
        int width = std::min(3LL, match.capturedLength());
#endif
        int value = (msecs % 1s).count() / divisor[width];
        QString text = StringUtil::intToPaddedString(value, width);
        fmt.replace(match.capturedStart(), match.capturedLength(), text);
    }

    if (negativeTime)
        fmt.prepend("-");

    return fmt;
}

}; // namespace MythDate
