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
    ISODate        = Qt::ISODate,   ///< Default UTC
    kFilename      = 0x000100,      ///< Default UTC, "yyyyMMddhhmmss"
    kDateFull      = 0x000200,      ///< Default local time
    kDateShort     = 0x000400,      ///< Default local time
    kDateEither    = kDateFull  | kDateShort,    ///< Default local time
    kTime          = 0x000800,      ///< Default local time
    kDateTimeFull  = kDateFull  | kTime, ///< Default local time
    kDateTimeShort = kDateShort | kTime, ///< Default local time
    kAddYear       = 0x001000,      ///< Add year to string if not included
    kSimplify      = 0x002000,      ///< Do Today/Yesterday/Tomorrow transform
    kDatabase      = 0x004000,      ///< Default UTC, database format
    kAutoYear      = 0x008000,      ///< Add year only if different from current year
    kScreenShotFilename = 0x010000,   ///< "yyyy-MM-ddThh-mm-ss.zzz"
    kRFC822        = 0x020000,      ///< HTTP Date format
    kOverrideUTC   = 0x100000,      ///< Present date/time in UTC
    kOverrideLocal = 0x200000,      ///< Present date/time in localtime
};

/// Returns current Date and Time in UTC.
/// \param stripped if true milliseconds are stripped
MBASE_PUBLIC QDateTime current(bool stripped = false);
/// Returns current Date and Time in UTC as a string.
/// \param stripped if true milliseconds are stripped
MBASE_PUBLIC QString current_iso_string(bool stripped = false);
/// Returns copy of QDateTime with TimeSpec set to UTC.
MBASE_PUBLIC QDateTime as_utc(const QDateTime &dt);

/// Converts kFilename && kISODate formats to QDateTime
MBASE_PUBLIC QDateTime fromString(const QString &dtstr);
/// Converts dy in format to QDateTime
MBASE_PUBLIC QDateTime fromString(const QString &dt, const QString &format);
MBASE_PUBLIC QDateTime fromSecsSinceEpoch(uint seconds);
/// Returns formatted string representing the time.
MBASE_PUBLIC QString toString(
    const QDateTime &datetime, uint format = MythDate::kDateTimeFull);
/// Warning: this function can not convert to and from UTC
MBASE_PUBLIC QString toString(
    QDate date, uint format = MythDate::kDateFull);

// Returns the total number of seconds since midnight
MBASE_PUBLIC std::chrono::seconds toSeconds( QTime time );

MBASE_PUBLIC std::chrono::milliseconds currentMSecsSinceEpochAsDuration(void);
MBASE_PUBLIC std::chrono::seconds secsInPast (const QDateTime& past);
MBASE_PUBLIC std::chrono::seconds secsInFuture (const QDateTime& future);

enum class FormatDurationUnit : int
{
    automatic = -1,
    ms,
    s,
    min,
    h,
    d,
};

/**
 * @brief Format a milliseconds duration.
 *
 * Convert a millisecond time value into a textual representation of the value
 * in the format "n day(s) HH:mm:ss.zzz" where any subset of consecutive elements
 * can be selected, omitting the rest.  All elements after the first are always
 * 0 padded.
 *
 * @param dur The time value in milliseconds. Since the type of this
 *     field is std::chrono::duration, any duration of a larger
 *     interval can be passed to this function and the compiler will
 *     convert it to milliseconds.
 *
 * @param min_unit The minimum sized unit that will be output.
 *                 Automatic is invalid at this time.
 * @param max_unit The maximum sized unit that will be output.  Automatic selects
 *                 the minimum required unit for a fully reduced representation.
 * @param padded   Whether or not the first entry should be padded with a 0.  The
 *                 days value is never padded.
 *
 * @todo Implement trimming trailing zeroes, i.e. enable automatic for min_unit?
 */
MBASE_PUBLIC QString formatDuration(std::chrono::milliseconds dur,
                                    FormatDurationUnit min_unit = FormatDurationUnit::s,
                                    FormatDurationUnit max_unit = FormatDurationUnit::automatic,
                                    bool padded = true);

} // namespace MythDate

#endif // MYTH_DATE_H
