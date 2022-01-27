#include "mythtimezone.h"

#include <cstdlib> // for malloc, getenv
#include <ctime>

#include <QDataStream>
#include <QTextStream>
#include <QDateTime>
#include <QTimeZone>
#include <QFileInfo>
#include <QDir>

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythdate.h"

#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
#define qEnvironmentVariable getenv
#endif

namespace MythTZ
{

int calc_utc_offset(void)
{
    QDateTime loc = QDateTime::currentDateTime();
    QDateTime utc = loc.toUTC();
    loc = QDateTime(loc.date(), loc.time(), Qt::UTC);
    return utc.secsTo(loc);
}

/** \fn getTimeZoneID()
 *  \brief Returns the zoneinfo time zone ID or as much time zone information
 *         as possible
 */
QString getTimeZoneID(void)
{
    QString zone_id("UNDEF");
#ifndef _WIN32
    // First, try the TZ environment variable to check for environment-specific
    // overrides
    QString tz = qEnvironmentVariable("TZ");
    if (tz.isEmpty())
    {
        // No TZ, so attempt to determine the system-configured time zone ID
        QDateTime dt = QDateTime::currentDateTime();
        tz = dt.timeZone().id();
    }

    if (!tz.isEmpty())
    {
        zone_id = tz;
        if (zone_id.startsWith("\"") || zone_id.startsWith("'"))
                zone_id.remove(0, 1);
        if (zone_id.endsWith("\"") || zone_id.endsWith("'"))
                zone_id.chop(1);
        if (zone_id.startsWith(":"))
            zone_id.remove(0, 1);
        // the "posix/" subdirectory typically contains the same files as the
        // "zoneinfo/" parent directory, but are not typically what are in use
        if (zone_id.startsWith("posix/"))
            zone_id.remove(0, 6);
    }
#else
    // Sadly, Windows zone names are different to the (probably Unix)
    // backend's names - "AUS Eastern Standard Time" vs "Australia/Sydney".
    // Translation is not worthwhile. Leave it as UNDEF to check the offset.
#endif
    return zone_id;
}

/** \fn checkTimeZone()
 *  \brief Verifies the time zone settings on this system agree with those
 *         on the master backend
 */
bool checkTimeZone(void)
{
    return true;
}

/// This overloaded version allows for the use of an existing (not managed by
/// MythContext) connection to the backend.
bool checkTimeZone(const QStringList &/*master_settings*/)
{
    return true;
}

}; // namespace MythTZ
