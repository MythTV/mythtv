#ifndef _MYTH_DATE_H_
#define _MYTH_DATE_H_

#include <QDateTime>
#include <QString>

#include "mythbaseexp.h"

namespace MythDate
{

typedef enum Formats
{
    ISODate        = Qt::ISODate,   ///< Default UTC
    kFilename      = 0x000100,      ///< Default UTC, "yyyyMMddhhmmss"
    kDateFull      = 0x000200,      ///< Default local time
    kDateShort     = 0x000400,      ///< Default local time
    kTime          = 0x000800,      ///< Default local time
    kDateTimeFull  = kDateFull  | kTime, ///< Default local time
    kDateTimeShort = kDateShort | kTime, ///< Default local time
    kAddYear       = 0x001000,      ///< Add year to string if not included
    kSimplify      = 0x002000,      ///< Do Today/Yesterday/Tomorrow transform
    kDatabase      = 0x004000,      ///< Default UTC, database format
    kScreenShotFilename = 0x8000,   ///< "yyyy-MM-ddThh-mm-ss.zzz"
    kOverrideUTC   = 0x100000,      ///< Present date/time in UTC
    kOverrideLocal = 0x200000,      ///< Present date/time in localtime
} Format;

/// Returns current Date and Time in UTC.
/// \param stripped if true milliseconds are stripped
MBASE_PUBLIC QDateTime current(bool stripped = false);
/// Returns current Date and Time in UTC as a string.
/// \param stripped if true milliseconds are stripped
MBASE_PUBLIC QString current_iso_string(bool stripped = false);
/// Returns copy of QDateTime with TimeSpec set to UTC.
MBASE_PUBLIC QDateTime as_utc(const QDateTime &dt);

/// Converts kFilename && kISODate formats to QDateTime
MBASE_PUBLIC QDateTime fromString(const QString&);
/// Converts dy in format to QDateTime
MBASE_PUBLIC QDateTime fromString(const QString &dt, const QString &format);
MBASE_PUBLIC QDateTime fromTime_t(uint seconds);
/// Returns formatted string representing the time.
MBASE_PUBLIC QString toString(
    const QDateTime &datetime, uint format = MythDate::kDateTimeFull);
/// Warning: this function can not convert to and from UTC
MBASE_PUBLIC QString toString(
    const QDate &date, uint format = MythDate::kDateFull);

};

#endif // _MYTH_DATE_H_
