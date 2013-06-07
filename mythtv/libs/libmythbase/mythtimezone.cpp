#include "mythtimezone.h"

#include <stdlib.h> // for malloc, getenv

#include <QDataStream>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythdate.h"

namespace MythTZ
{

int calc_utc_offset(void)
{
    QDateTime loc = QDateTime::currentDateTime();
    QDateTime utc = loc.toUTC();
    loc = QDateTime(loc.date(), loc.time(), Qt::UTC);
    return utc.secsTo(loc);
}

static bool compare_zone_files(QFileInfo first_file_info,
                               QFileInfo second_file_info)
{
    if (!first_file_info.isFile() || !second_file_info.isFile() ||
        !first_file_info.isReadable() || !second_file_info.isReadable())
        return false;

    qint64 first_file_size = first_file_info.size();
    // sanity check - zoneinfo files should typically be less than
    // about 4kB, but leave room for growth
    if ((first_file_size > 200 * 1024) ||
        (second_file_info.size() != first_file_size))
        return false;

    QFile first_file(first_file_info.absoluteFilePath());
    QByteArray first_file_data;
    first_file_data.resize(first_file_size);
    QFile second_file(second_file_info.absoluteFilePath());
    QByteArray second_file_data;
    second_file_data.resize(first_file_size);
    if (first_file.open(QIODevice::ReadOnly))
    {
        QDataStream in(&first_file);
        if (in.readRawData(first_file_data.data(),
                           first_file_size) != first_file_size)
        {
            first_file.close();
            return false;
        }
        first_file.close();
    }
    if (second_file.open(QIODevice::ReadOnly))
    {
        QDataStream in(&second_file);
        if (in.readRawData(second_file_data.data(),
                           first_file_size) != first_file_size)
        {
            second_file.close();
            return false;
        }
        second_file.close();
    }
    if (first_file_data == second_file_data)
        return true;

    return false;
}

#ifndef USING_MINGW
/* Helper function for getSystemTimeZoneID() that compares the
   zoneinfo_file_path (regular) file with files in the zoneinfo_dir_path until
   it finds a match.  The matching file's name is used to determine the time
   zone ID. */
static QString findZoneinfoFile(QString zoneinfo_file_path,
                                QString zoneinfo_dir_path)
{
    QString zone_id("UNDEF");
    QDir zoneinfo_dir(zoneinfo_dir_path);
    QFileInfoList dirlist = zoneinfo_dir.entryInfoList();
    QFileInfo info;
    QString basename;
    QFileInfo zoneinfo_file_info(zoneinfo_file_path);

    for (QFileInfoList::const_iterator it = dirlist.begin();
         it != dirlist.end(); ++it)
    {
        info = *it;
         // Skip '.' and '..' and other files starting with "." and
         // skip localtime (which is often a link to zoneinfo_file_path)
        basename = info.baseName();
        if (basename.isEmpty() || (basename == "localtime")) {
            continue;
        }
        if (info.isDir())
        {
            zone_id = findZoneinfoFile(zoneinfo_file_path,
                                       info.absoluteFilePath());
            if (zone_id != "UNDEF")
                return zone_id;
        }
        else if (compare_zone_files(zoneinfo_file_info, info))
        {
            zone_id = info.absoluteFilePath();
            break;
        }
    }
    return zone_id;
}
#endif

/* helper fuction to find the zone ID in a configuration string
   allows NIS-format /etc/timezone , which contains extra information:
   <time zone ID> <host or NIS domain name> # optional comments
*/
static bool parse_zone_id_config_string(QString& zone_id)
{
    bool found = false;
    QString zoneinfo_dir_path("/usr/share/zoneinfo/");
    QRegExp sep("\\s+");
    QFileInfo file_info;

    while (!found)
    {
        QString temp_zone_id = zone_id;
        temp_zone_id.replace(' ', '_');
        file_info.setFile(zoneinfo_dir_path + temp_zone_id);
        if (file_info.exists())
        {
            found = true;
        }
        else
        {
            zone_id = zone_id.section(sep, 0, -2);
            if (zone_id.isEmpty())
                break;
        }
    }
    return found;
}

/* helper fuction to read time zone id from a file
   Debian's /etc/timezone or Red Hat's /etc/sysconfig/clock */
static bool read_time_zone_id(QString filename, QString& zone_id)
{
    bool found = false;
    QFile file(filename);
    QFileInfo info(file);
    if (info.exists() && info.isFile() && info.isReadable())
    {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QString line;
            QTextStream in(&file);
            // Handle whitespace and quotes
            QRegExp re("^(?:ZONE\\s*=)?\\s*(['\"]?)([\\w\\s/-\\+]+)\\1\\s*(?:#.*)?$");
            re.setPatternSyntax(QRegExp::RegExp2);
            while (!in.atEnd())
            {
                line = in.readLine();
                if (re.indexIn(line) != -1)
                {
                    zone_id = re.cap(2);
                    if (parse_zone_id_config_string(zone_id))
                        found = true;
                    break;
                }
            }
            file.close();
        }
    }
    return found;
}

/* Helper function for getTimeZoneID() that provides an unprocessed time zone
   id obtained using system-dependent means of identifying the system's time
   zone. */
static QString getSystemTimeZoneID(void)
{
    QString zone_id("UNDEF");
#ifdef USING_MINGW
    // typedef struct _TIME_ZONE_INFORMATION { ...
    // GetTimeZoneInformation();
    // ...
    // Sadly, Windows zone names are different to the (probably Unix)
    // backend's names - "AUS Eastern Standard Time" vs "Australia/Sydney".
    // Translation is not worthwhile. Leave it as UNDEF to check the offset.
#else
    // Try to determine the time zone information by inspecting the system
    // configuration
    QString time_zone_file_path("/etc/timezone");
    QString clock_file_path("/etc/sysconfig/clock");
    QString zoneinfo_file_path("/etc/localtime");
    QString zoneinfo_dir_path("/usr/share/zoneinfo");

    // First, check time_zone_file_path (used by Debian-based systems)
    if (read_time_zone_id(time_zone_file_path, zone_id))
        return zone_id;

    // Next, look for the ZONE entry in clock_file_path (used by Red Hat-based
    // systems)
    if (read_time_zone_id(clock_file_path, zone_id))
        return zone_id;

    // Next check zoneinfo_file_path
    QFile zoneinfo_file(zoneinfo_file_path);
    QFileInfo info(zoneinfo_file);

    if (info.exists() && info.isFile())
    {
        QString tz;
        if (info.isSymLink())
        {
            // The symlink refers to a file whose name contains the zone ID
            tz = info.symLinkTarget();
        }
        else
        {
            // The zoneinfo_file is a copy of the file in the
            // zoneinfo_dir_path, so search for the same file in
            // zoneinfo_dir_path
            tz = findZoneinfoFile(zoneinfo_file_path, zoneinfo_dir_path);
        }
        if (tz != "UNDEF")
        {
            int pos = 0;
            // Get the zone ID from the filename
            // Look for the basename of zoneinfo_dir_path in case it's a
            // relative link
            QString zoneinfo_dirname = zoneinfo_dir_path.section('/', -1);
            if ((pos = tz.indexOf(zoneinfo_dirname)) != -1)
            {
                zone_id = tz.right(tz.size() - (pos + 1) -
                                   zoneinfo_dirname.size());
            }
        }
        else
        {
            // If we still haven't found a time zone, try localtime_r() to at
            // least get the zone name/abbreviation (as opposed to the
            // identifier for the set of rules governing the zone)
            char name[64];
            time_t t;
            struct tm *result = (struct tm *)malloc(sizeof(*result));

            if (result != NULL)
            {
                t = time(NULL);
                localtime_r(&t, result);
                if (result != NULL)
                {
                    if (strftime(name, sizeof(name), "%Z", result) > 0)
                        zone_id = name;
                    free(result);
                }
            }
        }
    }

#endif
    return zone_id;
}

/** \fn getTimeZoneID()
 *  \brief Returns the zoneinfo time zone ID or as much time zone information
 *         as possible
 */
QString getTimeZoneID(void)
{
    QString zone_id("UNDEF");
#ifndef USING_MINGW
    // First, try the TZ environment variable to check for environment-specific
    // overrides
    QString tz = getenv("TZ");
    if (tz.isEmpty())
    {
        // No TZ, so attempt to determine the system-configured time zone ID
        tz = getSystemTimeZoneID();
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
bool checkTimeZone(const QStringList &master_settings)
{
    return true;
}

}; // namespace MythTZ
