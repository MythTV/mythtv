#ifndef MYTH_DATE_H
#define MYTH_DATE_H

#include <chrono>
using namespace std::chrono_literals;

#include <QDateTime>
#include <QString>

#include "mythbaseexp.h"

namespace MythDate
{

enum Format
{
    ISODate        = Qt::ISODate,               ///< Default UTC
    kFilename      = 0x000100,                  ///< Default UTC, "yyyyMMddhhmmss"
    kDateFull      = 0x000200,                  ///< Default local time
    kDateShort     = 0x000400,                  ///< Default local time
    kDateEither    = kDateFull  | kDateShort,   ///< Default local time
    kTime          = 0x000800,                  ///< Default local time
    kDateTimeFull  = kDateFull  | kTime,        ///< Default local time
    kDateTimeShort = kDateShort | kTime,        ///< Default local time
    kAddYear       = 0x001000,      ///< Add year to string if not included
    kSimplify      = 0x002000,      ///< Do Today/Yesterday/Tomorrow transform
    kDatabase      = 0x004000,      ///< Default UTC, database format
    kAutoYear      = 0x008000,      ///< Add year only if different from current year
    kScreenShotFilename = 0x010000, ///< "yyyy-MM-ddThh-mm-ss.zzz"
    kRFC822        = 0x020000,      ///< HTTP Date format
    kOverrideUTC   = 0x100000,      ///< Present date/time in UTC
    kOverrideLocal = 0x200000,      ///< Present date/time in localtime
};

/// Returns current Date and Time in UTC.
/// \param truncateToSeconds if true, time is set to the beginning of the current second, i.e. milliseconds = 0
inline QDateTime current(bool truncateToSeconds = false)
{
    QDateTime dt = QDateTime::currentDateTimeUtc();
    if (truncateToSeconds)
        dt = dt.addMSecs(-dt.time().msec());
    return dt;
}

/**
\brief Returns the current Date and Time in UTC as an ISO 8601 extended format string.
\param no_ms if true, the string does not include milliseconds

The string format is (no_ms) ? "yyyy-MM-ddTHH:mm:ssZ" : "yyyy-MM-ddTHH:mm:ss.zzzZ"
*/
inline QString current_iso_string(bool no_ms = false)
{
    return no_ms ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) :
                   QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

/// Returns copy of QDateTime with TimeSpec set to UTC.
/// This reinterprets (changes) the time represented.
inline QDateTime as_utc(const QDateTime &old_dt)
{
    QDateTime dt(old_dt);
    dt.setTimeSpec(Qt::UTC);
    return dt;
}

/// Returns formatted string representing the time.
MBASE_PUBLIC QString toString(const QDateTime &datetime, uint format = MythDate::kDateTimeFull);

/// Warning: this function can not convert to and from UTC
MBASE_PUBLIC QString toString(QDate date, uint format = MythDate::kDateFull);

/// Converts kFilename && kISODate formats to QDateTime
inline QDateTime fromString(const QString &dtstr)
{
    QDateTime dt;
    if (dtstr.isEmpty())
        return as_utc(dt); // null and invalid

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

/// Converts a UTC string with format to a UTC QDateTime
inline QDateTime fromString(const QString &str, const QString &format)
{
    return as_utc(QDateTime::fromString(str, format));
}

/**
 *  This function takes the number of seconds since the start of the
 *  epoch and returns a QDateTime with the equivalent value.
 *
 *  Note: This function returns a QDateTime set to UTC, whereas the
 *  QDateTime::fromSecsSinceEpoch function returns a value set to
 *  localtime.
 *
 *  Note: QDateTime does not account for leap seconds.  Neither does Unix time,
 *  effectively, since it specifies a day is always exactly 86 400 seconds.
 *
 *  \param seconds  The number of seconds since the start of the epoch
 *                  at Jan 1 1970 at 00:00:00.
 *  \return A QDateTime.
 */
inline QDateTime fromSecsSinceEpoch(uint seconds)
{
    return QDateTime::fromSecsSinceEpoch(seconds, Qt::UTC);
}

/** \brief Returns the total number of seconds since midnight of the supplied QTime
 *
 *  \param time     The QTime object to use
 */
inline std::chrono::seconds toSeconds(QTime time)
{
    if (!time.isValid())
        return 0s;

    std::chrono::seconds secs = std::chrono::hours(time.hour());
    secs += std::chrono::minutes(time.minute());
    secs += std::chrono::seconds(time.second());

    return secs;
}

inline std::chrono::milliseconds currentMSecsSinceEpochAsDuration(void)
{
    return std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch());
}

inline std::chrono::seconds secsInPast(const QDateTime& past)
{
    return std::chrono::seconds(past.secsTo(MythDate::current()));
}

inline std::chrono::seconds secsInFuture(const QDateTime& future)
{
    return std::chrono::seconds(MythDate::current().secsTo(future));
}

/**
 * \brief Format a milliseconds time value
 *
 * Convert a millisecond time value into a textual representation of the value.
 *
 * \param msecs The time value in milliseconds. Since the type of this
 *     field is std::chrono::duration, any duration of a larger
 *     interval can be passed to this function and the compiler will
 *     convert it to milliseconds.
 *
 * \param fmt A formatting string specifying how to output the time.
 *     See QTime::toString for the a definition fo valid formatting
 *     characters.
 */
inline QString formatTime(std::chrono::milliseconds msecs, const QString& fmt)
{
    return QTime::fromMSecsSinceStartOfDay(msecs.count()).toString(fmt);
}

} // namespace MythDate

#endif // MYTH_DATE_H
