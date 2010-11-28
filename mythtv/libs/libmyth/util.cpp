
#include "util.h"

// C++ headers
#include <iostream>

using namespace std;

// C headers
#include <cerrno>
#include <stdlib.h>
#include <time.h>

// POSIX
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>

// System specific C headers
#include "compat.h"

#ifdef linux
#include <sys/vfs.h>
#include <sys/sysinfo.h>
#endif

#if CONFIG_DARWIN
#include <mach/mach.h>
#endif

#ifdef BSD
#include <sys/mount.h>  // for struct statfs
#include <sys/sysctl.h>
#endif

// Qt headers
#include <QFile>
#include <QDir>
#include <QFileInfo>

// Myth headers
#include "mythcorecontext.h"
#include "exitcodes.h"
#include "mythxdisplay.h"
#include "mythmediamonitor.h"
#include "mythverbose.h"
#include "msocketdevice.h"
#include "mythsocket.h"
#include "mythcoreutil.h"
#include "remotefile.h"

#include "mythconfig.h" // for CONFIG_DARWIN

/** \fn mythCurrentDateTime()
 *  \brief Returns the current QDateTime object, stripped of its msec component
 */
QDateTime mythCurrentDateTime()
{
    QDateTime rettime = QDateTime::currentDateTime();
    QTime orig = rettime.time();
    rettime.setTime(orig.addMSecs(-orig.msec()));
    return rettime;
}

QDateTime myth_dt_from_string(const QString &dtstr)
{
    if (dtstr.isEmpty())
        return QDateTime();

    if (!dtstr.contains("-") && dtstr.length() == 14)
    {
        // must be in YYYYMMDDhhmmss format, convert to ISODate
        QString isodate =
            QString("%1-%2-%3T%4:%5:%6")
            .arg(dtstr.mid( 0,4), 4, QLatin1Char('0'))
            .arg(dtstr.mid( 4,2), 2, QLatin1Char('0'))
            .arg(dtstr.mid( 6,2), 2, QLatin1Char('0'))
            .arg(dtstr.mid( 8,2), 2, QLatin1Char('0'))
            .arg(dtstr.mid(10,2), 2, QLatin1Char('0'))
            .arg(dtstr.mid(12,2), 2, QLatin1Char('0'));
        return QDateTime::fromString(isodate, Qt::ISODate);
    }

    return QDateTime::fromString(dtstr, Qt::ISODate);
}

int calc_utc_offset(void)
{
    QDateTime loc = QDateTime::currentDateTime();
    QDateTime utc = QDateTime::currentDateTime().toUTC();

    int utc_offset = MythSecsTo(utc, loc);

    // clamp to nearest minute if within 10 seconds
    int off = utc_offset % 60;
    if (abs(off) < 10)
        utc_offset -= off;
    if (off < -50 && off > -60)
        utc_offset -= 60 + off;
    if (off > +50 && off < +60)
        utc_offset += 60 - off;

    return utc_offset;
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
         it != dirlist.end(); it++)
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

/* Helper function for checkTimeZone() that compares zone ID's.
   In the event that the zone ID's differ, checks to see if the local
   zoneinfo database has both zone ID's and if they're equivalent. */
static bool compare_zone_IDs(QString firstZoneID, QString secondZoneID)
{
    // Some distros use spaces rather than underscores in the zone ID, so
    // allow matches where the only difference is space vs. underscore.
    firstZoneID.replace(' ', '_');
    secondZoneID.replace(' ', '_');
    if (firstZoneID == secondZoneID)
        return true;

    // Although the zone ID names don't match, they may refer to equivalent
    // rules, so compare the files
    QString zoneinfo_dir_path("/usr/share/zoneinfo");
    QFileInfo firstInfo(zoneinfo_dir_path + "/" + firstZoneID);
    QFileInfo secondInfo(zoneinfo_dir_path + "/" + secondZoneID);
    if (compare_zone_files(firstInfo, secondInfo))
        return true;

    return false;
}

static void print_timezone_info(QString master_zone_id, QString local_zone_id,
                                int master_utc_offset, int local_utc_offset,
                                QString master_time, QString local_time)
{
    VERBOSE(VB_IMPORTANT, QString("Detected time zone settings:\n"
"    Master: Zone ID: '%1', UTC Offset: '%2', Current Time: '%3'\n"
"     Local: Zone ID: '%4', UTC Offset: '%5', Current Time: '%6'\n")
            .arg(master_zone_id).arg(master_utc_offset).arg(master_time)
            .arg(local_zone_id).arg(local_utc_offset).arg(local_time));
}

/** \fn checkTimeZone()
 *  \brief Verifies the time zone settings on this system agree with those
 *         on the master backend
 */
bool checkTimeZone(void)
{
    if (gCoreContext->IsMasterBackend())
        return true;

    QStringList master_settings(QString("QUERY_TIME_ZONE"));
    if (!gCoreContext->SendReceiveStringList(master_settings))
    {
        VERBOSE(VB_IMPORTANT, "Unable to determine master backend time zone "
                              "settings.  If those settings differ from local "
                              "settings, some functionality will fail.");
        return true;
    }

    return checkTimeZone(master_settings);
}

/// This overloaded version allows for the use of an existing (not managed by
/// MythContext) connection to the backend.
bool checkTimeZone(const QStringList &master_settings)
{
    QDateTime local_time = mythCurrentDateTime();
    QString local_time_string = local_time.toString(Qt::ISODate);

    bool have_zone_IDs = true;

    QString master_time_zone_ID = master_settings[0];
    int master_utc_offset       = master_settings[1].toInt();
    QString master_time_string  = master_settings[2];
    QString local_time_zone_ID  = getTimeZoneID();
    int local_utc_offset        = calc_utc_offset();

    if (master_time_zone_ID == "UNDEF")
    {
        VERBOSE(VB_IMPORTANT, "Unable to determine master backend time zone "
                              "settings. If local time zone settings differ "
                              "from master backend settings, some "
                              "functionality will fail.");
        have_zone_IDs = false;
    }
    if (local_time_zone_ID == "UNDEF")
    {
        VERBOSE(VB_IMPORTANT, "Unable to determine local time zone settings. "
                              "If local time zone settings differ from "
                              "master backend settings, some functionality "
                              "will fail.");
        have_zone_IDs = false;
    }

    if (have_zone_IDs &&
        !compare_zone_IDs(master_time_zone_ID, local_time_zone_ID))
    {
        VERBOSE(VB_IMPORTANT, "Time zone settings on the master backend "
                              "differ from those on this system.");
        print_timezone_info(master_time_zone_ID, local_time_zone_ID,
                            master_utc_offset, local_utc_offset,
                            master_time_string, local_time_string);
        return false;
    }

    // Verify offset
    if (master_utc_offset != local_utc_offset)
    {
        VERBOSE(VB_IMPORTANT, "UTC offset on the master backend differs "
                              "from offset on this system.");
        print_timezone_info(master_time_zone_ID, local_time_zone_ID,
                            master_utc_offset, local_utc_offset,
                            master_time_string, local_time_string);
        return false;
    }

    // Verify current time
    if (master_time_string == "UNDEF")
    {
        VERBOSE(VB_IMPORTANT, "Unable to determine current time on the master "
                              "backend . If local time or time zone settings "
                              "differ from those on the master backend, some "
                              "functionality will fail.");
    }
    else
    {
        QDateTime master_time = QDateTime::fromString(master_time_string,
                                                      Qt::ISODate);
        uint timediff = abs(master_time.secsTo(local_time));
        if (timediff > 300)
        {
            VERBOSE(VB_IMPORTANT, "Current time on the master backend "
                                  "differs from time on this system.");
            print_timezone_info(master_time_zone_ID, local_time_zone_ID,
                                master_utc_offset, local_utc_offset,
                                master_time_string, local_time_string);
            return false;
        }
        else if (timediff > 20)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Warning! Time difference between the master "
                            "backend and this system is %1 seconds.")
                    .arg(timediff));
        }
    }

    return true;
}

/** \fn MythSecsTo(const QDateTime&, const QDateTime&)
 *  \brief Returns "'to' - 'from'" for two QDateTime's in seconds.
 */
int MythSecsTo(const QDateTime &from, const QDateTime &to)
{
   return (from.time().secsTo(to.time()) +
           from.date().daysTo(to.date()) * 60 * 60 * 24);
}

/** \fn MythUTCToLocal(const QDateTime&)
 *  \brief Converts a QDateTime in UTC to local time.
 */
QDateTime MythUTCToLocal(const QDateTime &utc)
{
    QDateTime local = QDateTime(QDate(1970, 1, 1));

    int timesecs = MythSecsTo(local, utc);
    QDateTime localdt;
    localdt.setTime_t(timesecs);

    return localdt;
}

/** \fn getUptime(time_t&)
 *  \brief Returns uptime statistics.
 *  \todo Update Statistics are not supported (by MythTV) on NT or DOS.
 *  \return true if successful, false otherwise.
 */
bool getUptime(time_t &uptime)
{
#ifdef __linux__
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) == -1)
    {
        VERBOSE(VB_IMPORTANT, "sysinfo() error");
        return false;
    }
    else
        uptime = sinfo.uptime;

#elif defined(__FreeBSD__) || CONFIG_DARWIN

    int            mib[2];
    struct timeval bootTime;
    size_t         len;

    // Uptime is calculated. Get this machine's boot time
    // and subtract it from the current machine time
    len    = sizeof(bootTime);
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;
    if (sysctl(mib, 2, &bootTime, &len, NULL, 0) == -1)
    {
        VERBOSE(VB_IMPORTANT, "sysctl() error");
        return false;
    }
    else
        uptime = time(NULL) - bootTime.tv_sec;
#elif defined(USING_MINGW)
    uptime = ::GetTickCount() / 1000;
#else
    // Hmmm. Not Linux, not FreeBSD or Darwin. What else is there :-)
    VERBOSE(VB_IMPORTANT, "Unknown platform. How do I get the uptime?");
    return false;
#endif

    return true;
}

/** \fn getMemStats(int&,int&,int&,int&)
 *  \brief Returns memory statistics in megabytes.
 *
 *  \todo Memory Statistics are not supported (by MythTV) on NT or DOS.
 *  \return true if it succeeds, false otherwise.
 */
bool getMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM)
{
#ifdef __linux__
    size_t MB = (1024*1024);
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) == -1)
    {
        VERBOSE(VB_IMPORTANT, "getMemStats(): Error, sysinfo() call failed.");
        return false;
    }
    else
        totalMB = (int)((sinfo.totalram  * sinfo.mem_unit)/MB),
        freeMB  = (int)((sinfo.freeram   * sinfo.mem_unit)/MB),
        totalVM = (int)((sinfo.totalswap * sinfo.mem_unit)/MB),
        freeVM  = (int)((sinfo.freeswap  * sinfo.mem_unit)/MB);

#elif CONFIG_DARWIN
    mach_port_t             mp;
    mach_msg_type_number_t  count;
    vm_size_t               pageSize;
    vm_statistics_data_t    s;

    mp = mach_host_self();

    // VM page size
    if (host_page_size(mp, &pageSize) != KERN_SUCCESS)
        pageSize = 4096;   // If we can't look it up, 4K is a good guess

    count = HOST_VM_INFO_COUNT;
    if (host_statistics(mp, HOST_VM_INFO,
                        (host_info_t)&s, &count) != KERN_SUCCESS)
    {
        VERBOSE(VB_IMPORTANT, "getMemStats(): Error, "
                "failed to get virtual memory statistics.");
        return false;
    }

    pageSize >>= 10;  // This gives usages in KB
    totalMB = (s.active_count + s.inactive_count
               + s.wire_count + s.free_count) * pageSize / 1024;
    freeMB  = s.free_count * pageSize / 1024;


    // This is a real hack. I have not found a way to ask the kernel how much
    // swap it is using, and the dynamic_pager daemon doesn't even seem to be
    // able to report what filesystem it is using for the swapfiles. So, we do:
    long long total, used, free;
    free = getDiskSpace("/private/var/vm", total, used);
    totalVM = (int)(total/1024LL), freeVM = (int)(free/1024LL);

#else
    VERBOSE(VB_IMPORTANT, "getMemStats(): Unknown platform. "
            "How do I get the memory stats?");
    return false;
#endif

    return true;
}

/**
 * \brief Eject a disk, unmount a drive, open a tray
 *
 * If the Media Monitor is enabled, we use its fully-featured routine.
 * Otherwise, we guess a drive and use a primitive OS-specific command
 */
void myth_eject()
{
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon)
        mon->ChooseAndEjectMedia();
    else
    {
        VERBOSE(VB_MEDIA, "CD/DVD Monitor isn't enabled.");
#ifdef __linux__
        VERBOSE(VB_MEDIA, "Trying Linux 'eject -T' command");
        myth_system("eject -T");
#elif CONFIG_DARWIN
        VERBOSE(VB_MEDIA, "Trying 'disktool -e disk1");
        myth_system("disktool -e disk1");
#endif
    }
}

/**
 * \brief Quess whether a string is UTF-8
 *
 * \note  This does not attempt to \e validate the whole string.
 *        It just checks if it has any UTF-8 sequences in it.
 */

bool hasUtf8(const char *str)
{
    const uchar *c = (uchar *) str;

    while (*c++)
    {
        // ASCII is < 0x80.
        // 0xC2..0xF4 is probably UTF-8.
        // Anything else probably ISO-8859-1 (Latin-1, Unicode)

        if (*c > 0xC1 && *c < 0xF5)
        {
            int bytesToCheck = 2;  // Assume  0xC2-0xDF (2 byte sequence)

            if (*c > 0xDF)         // Maybe   0xE0-0xEF (3 byte sequence)
                ++bytesToCheck;
            if (*c > 0xEF)         // Matches 0xF0-0xF4 (4 byte sequence)
                ++bytesToCheck;

            while (bytesToCheck--)
            {
                ++c;

                if (! *c)                    // String ended in middle
                    return false;            // Not valid UTF-8

                if (*c < 0x80 || *c > 0xBF)  // Bad UTF-8 sequence
                    break;                   // Keep checking in outer loop
            }

            if (!bytesToCheck)  // Have checked all the bytes in the sequence
                return true;    // Hooray! We found valid UTF-8!
        }
    }

    return false;
}

/**
 * \brief Can we ping host within timeout seconds?
 */
bool ping(const QString &host, int timeout)
{
#ifdef USING_MINGW
    QString cmd = QString("%systemroot%\\system32\\ping.exe -i %1 -n 1 %2>NUL")
                  .arg(timeout).arg(host);

    if (myth_system(cmd, kMSDontBlockInputDevs | kMSDontDisableDrawing |
                         kMSProcessEvents))
        return false;
#else
    QString cmd = QString("ping -t %1 -c 1  %2  >/dev/null 2>&1")
                  .arg(timeout).arg(host);

    if (myth_system(cmd, kMSDontBlockInputDevs | kMSDontDisableDrawing |
                         kMSProcessEvents))
    {
        // ping command may not like -t argument. Simplify:

        cmd = QString("ping -c 1  %2  >/dev/null 2>&1").arg(host);

        if (myth_system(cmd, kMSDontBlockInputDevs | kMSDontDisableDrawing |
                             kMSProcessEvents))
            return false;
    }
#endif

    return true;
}

/**
 * \brief Can we talk to port on host?
 */
bool telnet(const QString &host, int port)
{
    MythSocket *s = new MythSocket();

    bool connected = s->connect(host, port);
    s->DownRef();

    return connected;
}

/** \fn copy(QFile&,QFile&,uint)
 *  \brief Copies src file to dst file.
 *
 *   If the dst file is open, it must be open for writing.
 *   If the src file is open, if must be open for reading.
 *
 *   The files will be in the same open or close state after
 *   this function runs as they were prior to this function being called.
 *
 *   This function does not care if the files are actual files.
 *   For compatibility with pipes and socket streams the file location
 *   will not be reset to 0 at the end of this function. If the function
 *   is successful the file pointers will be at the end of the copied
 *   data.
 *
 *  \param dst Destination QFile
 *  \param src Source QFile
 *  \param block_size Optional block size in bytes, must be at least 1024,
 *                    otherwise the default of 16 KB will be used.
 *  \return bytes copied on success, -1 on failure.
 */
long long copy(QFile &dst, QFile &src, uint block_size)
{
    uint buflen = (block_size < 1024) ? (16 * 1024) : block_size;
    char *buf = new char[buflen];
    bool odst = false, osrc = false;

    if (!buf)
        return -1LL;

    if (!dst.isWritable() && !dst.isOpen())
        odst = dst.open(QIODevice::Unbuffered|QIODevice::WriteOnly|QIODevice::Truncate);

    if (!src.isReadable() && !src.isOpen())
        osrc = src.open(QIODevice::Unbuffered|QIODevice::ReadOnly);

    bool ok = dst.isWritable() && src.isReadable();
    long long total_bytes = 0LL;
    while (ok)
    {
        long long rlen, wlen, off = 0;
        rlen = src.read(buf, buflen);
        if (rlen<0)
        {
            VERBOSE(VB_IMPORTANT, "util.cpp:copy: read error");
            ok = false;
            break;
        }
        if (rlen==0)
            break;

        total_bytes += (long long) rlen;

        while ((rlen-off>0) && ok)
        {
            wlen = dst.write(buf + off, rlen - off);
            if (wlen>=0)
                off+= wlen;
            if (wlen<0)
            {
                VERBOSE(VB_IMPORTANT, "util.cpp:copy: write error");
                ok = false;
            }
        }
    }
    delete[] buf;

    if (odst)
        dst.close();

    if (osrc)
        src.close();

    return (ok) ? total_bytes : -1LL;
}

QString createTempFile(QString name_template, bool dir)
{
    int ret = -1;

#ifdef USING_MINGW
    char temppath[MAX_PATH] = ".";
    char tempfilename[MAX_PATH] = "";
    // if GetTempPath fails, use current dir
    GetTempPathA(MAX_PATH, temppath);
    if (GetTempFileNameA(temppath, "mth", 0, tempfilename))
    {
        if (dir)
        {
            // GetTempFileNameA creates the file, so delete it before mkdir
            unlink(tempfilename);
            ret = mkdir(tempfilename);
        }
        else
            ret = open(tempfilename, O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
    }
    QString tmpFileName(tempfilename);
#else
    QByteArray safe_name_template = name_template.toAscii();
    const char *tmp = safe_name_template.constData();
    char *ctemplate = strdup(tmp);

    if (dir)
    {
        ret = (mkdtemp(ctemplate)) ? 0 : -1;
    }
    else
    {
        mode_t cur_umask = umask(S_IRWXO | S_IRWXG);
        ret = mkstemp(ctemplate);
        umask(cur_umask);
    }

    QString tmpFileName(ctemplate);
    free(ctemplate);
#endif

    if (ret == -1)
    {
        VERBOSE(VB_IMPORTANT, QString("createTempFile(%1), Error ")
                .arg(name_template) + ENO);
        return name_template;
    }

    if (!dir && (ret >= 0))
        close(ret);

    return tmpFileName;
}

/** \fn makeFileAccessible(QString)
 *  \brief Makes a file accessible to all frontends/backends.
 *
 *   This function abstracts the functionality of making a file accessible to
 *   all frontends and backends.  Currently it contains a permissions hack that
 *   makes a file accessible even on a system with an improperly configured
 *   environment (umask/group) where the frontend and backend are being run as
 *   different users or where a NFS share is used but UID's/GID's differ on
 *   different hosts.
 *
 *   Though the function currently only changes the file mode to 0666, by
 *   abstracting the functionality, it will be easier to make changes in the
 *   future if a better approach is chosen.  Similarly, using this function
 *   allows the hack to be applied only when required if code is written to
 *   detect or allow the user to specify their system is misconfigured.
 *
 *  \param filename   Path of file to make accessible
 */
void makeFileAccessible(QString filename)
{
    QByteArray fname = filename.toAscii();
    chmod(fname.constData(), 0666);
}

/**
 * In an interactive shell, prompt the user to input a string
 */
QString getResponse(const QString &query, const QString &def)
{
    QByteArray tmp = query.toLocal8Bit();
    cout << tmp.constData();

    tmp = def.toLocal8Bit();
    if (def.size())
        cout << " [" << tmp.constData() << "]  ";
    else
        cout << "  ";

    if (!isatty(fileno(stdin)) || !isatty(fileno(stdout)))
    {
        cout << endl << "[console is not interactive, using default '"
             << tmp.constData() << "']" << endl;
        return def;
    }

    char response[80];
    cin.clear();
    cin.getline(response, 80);
    if (!cin.good())
    {
        cout << endl;
        VERBOSE(VB_IMPORTANT, "Read from stdin failed");
        return NULL;
    }

    QString qresponse = response;

    if (qresponse.isEmpty())
        qresponse = def;

    return qresponse;
}

/**
 * In an interactive shell, prompt the user to input a number
 */
int intResponse(const QString &query, int def)
{
    QString str_resp = getResponse(query, QString("%1").arg(def));
    if (str_resp.isEmpty())
        return def;
    bool ok;
    int resp = str_resp.toInt(&ok);
    return (ok ? resp : def);
}


QString getSymlinkTarget(const QString &start_file,
                         QStringList   *intermediaries,
                         unsigned       maxLinks)
{
#if 0
    VERBOSE(VB_IMPORTANT,
            QString("getSymlinkTarget('%1', 0x%2, %3)")
            .arg(start_file).arg((uint64_t)intermediaries,0,16)
            .arg(maxLinks));
#endif

    QString   link     = QString::null;
    QString   cur_file = start_file; cur_file.detach();
    QFileInfo fi(cur_file);

    if (intermediaries)
    {
        intermediaries->clear();
        intermediaries->push_back(start_file);
    }

    for (uint i = 0; (i <= maxLinks) && fi.isSymLink() &&
             !(link = fi.readLink()).isEmpty(); i++)
    {
        cur_file = (link[0] == '/') ?
            link : // absolute link
            fi.absoluteDir().absolutePath() + "/" + link; // relative link

        if (intermediaries && !intermediaries->contains(cur_file))
            intermediaries->push_back(cur_file);

        fi = QFileInfo(cur_file);
    }

    if (intermediaries)
        intermediaries->detach();

#if 0
    if (intermediaries)
    {
        for (uint i = 0; i < intermediaries->size(); i++)
        {
            VERBOSE(VB_IMPORTANT, QString("    inter%1: %2")
                    .arg(i).arg((*intermediaries)[i]));
        }
    }

    VERBOSE(VB_IMPORTANT,
            QString("getSymlinkTarget() -> '%1'")
            .arg((!fi.isSymLink()) ? cur_file : QString::null));
#endif

    return (!fi.isSymLink()) ? cur_file : QString::null;
}

void sendPlaybackStart(void)
{
    MythEvent me(QString("PLAYBACK_START %1").arg(gCoreContext->GetHostName()));
    gCoreContext->dispatchNow(me);
}

void sendPlaybackEnd(void)
{
    MythEvent me(QString("PLAYBACK_END %1").arg(gCoreContext->GetHostName()));
    gCoreContext->dispatchNow(me);
}

bool IsMACAddress(QString MAC)
{
    QStringList tokens = MAC.split(':');
    if (tokens.size() != 6)
    {
        VERBOSE(VB_NETWORK,
            QString("IsMACAddress(%1) = false, doesn't have 6 parts").arg(MAC));
        return false;
    }

    int y;
    bool ok;
    int value;
    for (y = 0; y < 6; y++)
    {
        if (tokens[y].isEmpty())
        {
            VERBOSE(VB_NETWORK, QString("IsMACAddress(%1) = false, part #%2 "
                    "is empty.").arg(MAC).arg(y));
            return false;
        }

        value = tokens[y].toInt(&ok, 16);
        if (!ok)
        {
            VERBOSE(VB_NETWORK, QString("IsMACAddress(%1) = false, unable to "
                    "convert part '%2' to integer.").arg(MAC).arg(tokens[y]));
            return false;
        }

        if (value > 255)
        {
            VERBOSE(VB_NETWORK, QString("IsMACAddress(%1) = false, part #%2 "
                    "evaluates to %3 which is higher than 255.")
                    .arg(MAC).arg(y).arg(value));
            return false;
        }
    }

    VERBOSE(VB_NETWORK, QString("IsMACAddress(%1) = true").arg(MAC));
    return true;
}

QString FileHash(QString filename)
{
    QFile file(filename);
    QFileInfo fileinfo(file);
    qint64 initialsize = fileinfo.size();
    quint64 hash = 0;

    if (initialsize == 0)
        return QString("NULL");

    if (file.open(QIODevice::ReadOnly))
        hash = initialsize;
    else
    {
        VERBOSE(VB_GENERAL, QString("Error: Unable to open "
                "selected file, missing read permissions?"));
        return QString("NULL");
    }

    file.seek(0);
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    for (quint64 tmp = 0, i = 0; i < 65536/sizeof(tmp); i++)
    {
        stream >> tmp;
        hash += tmp;
    }

    file.seek(initialsize - 65536);
    for (quint64 tmp = 0, i = 0; i < 65536/sizeof(tmp); i++)
    {
        stream >> tmp;
        hash += tmp;
    }

    file.close();

    QString output = QString("%1").arg(hash, 0, 16);
    return output;
}

bool WakeOnLAN(QString MAC)
{
    char msg[1024] = "\xFF\xFF\xFF\xFF\xFF\xFF";
    int  msglen = 6;
    int  x, y;
    QStringList tokens = MAC.split(':');
    int macaddr[6];
    bool ok;

    if (tokens.size() != 6)
    {
        VERBOSE(VB_IMPORTANT,
                QString( "WakeOnLan(%1): Incorrect MAC length").arg(MAC));
        return false;
    }

    for (y = 0; y < 6; y++)
    {
        macaddr[y] = tokens[y].toInt(&ok, 16);

        if (!ok)
        {
            VERBOSE(VB_IMPORTANT,
                    QString( "WakeOnLan(%1): Invalid MAC address").arg(MAC));
            return false;
        }
    }

    for (x = 0; x < 16; x++)
        for (y = 0; y < 6; y++)
            msg[msglen++] = macaddr[y];

    VERBOSE(VB_NETWORK,
            QString("WakeOnLan(): Sending WOL packet to %1").arg(MAC));

    MSocketDevice socket(MSocketDevice::Datagram);
    socket.setBroadcast(true);
    socket.writeBlock(msg, msglen, QHostAddress("255.255.255.255"), 32767);

    return true;
}

bool IsPulseAudioRunning(void)
{
#ifdef USING_MINGW
    return false;
#endif

#if CONFIG_DARWIN || (__FreeBSD__) || defined(__OpenBSD__)
    const char *command = "ps -ax | grep -i pulseaudio | grep -v grep > /dev/null";
#else
    const char *command = "ps -ae | grep pulseaudio > /dev/null";
#endif
    // Do NOT use kMSProcessEvents here, it will cause deadlock
    bool res = myth_system(command, kMSDontBlockInputDevs | 
                                    kMSDontDisableDrawing);
    return !res;
}

bool myth_nice(int val)
{
    errno = 0;
    int ret = nice(val);

    if ((-1 == ret) && (0 != errno) && (val >= 0))
    {
        VERBOSE(VB_IMPORTANT, "Failed to nice process" + ENO);
        return false;
    }

    return true;
}

void myth_yield(void)
{
#ifdef _POSIX_PRIORITY_SCHEDULING
    if (sched_yield()<0)
        usleep(5000);
#else
    usleep(5000);
#endif
}

/** \brief Allows setting the I/O priority of the current process/thread.
 *
 *  As of November 14th, 2010 this only works on Linux when using the CFQ
 *  I/O Scheduler. The range is -1 to 8, with -1 being real-time priority
 *  0 through 7 being standard best-time priorities and 8 being the idle
 *  priority. The deadline and noop I/O Schedulers will ignore this but
 *  are much less likely to starve video playback to feed the transcoder
 *  or flagger. (noop is only recommended for SSDs.)
 *
 *  Since a process needs to have elevated priviledges to use either the
 *  real-time or idle priority this will try priorities 0 or 7 respectively
 *  if -1 or 8 do not work. It will not report an error on these conditions
 *  as they will be the common case.
 *
 *  Only Linux on i386, ppc, x86_64 and ia64 are currently supported.
 *  This is a no-op on all other architectures and platforms.
 */
#if defined(__linux__) && ( defined(__i386__) || defined(__ppc__) || \
                            defined(__x86_64__) || defined(__ia64__) )

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <asm/unistd.h>

#if defined(__i386__)
# define __NR_ioprio_set  289
# define __NR_ioprio_get  290
#elif defined(__ppc__)
# define __NR_ioprio_set  273
# define __NR_ioprio_get  274
#elif defined(__x86_64__)
# define __NR_ioprio_set  251
# define __NR_ioprio_get  252
#elif defined(__ia64__)
# define __NR_ioprio_set  1274
# define __NR_ioprio_get  1275
#endif

#define IOPRIO_BITS             (16)
#define IOPRIO_CLASS_SHIFT      (13)
#define IOPRIO_PRIO_MASK        ((1UL << IOPRIO_CLASS_SHIFT) - 1)
#define IOPRIO_PRIO_CLASS(mask) ((mask) >> IOPRIO_CLASS_SHIFT)
#define IOPRIO_PRIO_DATA(mask)  ((mask) & IOPRIO_PRIO_MASK)
#define IOPRIO_PRIO_VALUE(class, data)  (((class) << IOPRIO_CLASS_SHIFT) | data)

enum { IOPRIO_CLASS_NONE,IOPRIO_CLASS_RT,IOPRIO_CLASS_BE,IOPRIO_CLASS_IDLE, };
enum { IOPRIO_WHO_PROCESS = 1, IOPRIO_WHO_PGRP, IOPRIO_WHO_USER, };

bool myth_ioprio(int val)
{
    int new_ioclass = (val < 0) ? IOPRIO_CLASS_RT :
        (val > 7) ? IOPRIO_CLASS_IDLE : IOPRIO_CLASS_BE;
    int new_iodata = (new_ioclass == IOPRIO_CLASS_BE) ? val : 0;
    int new_ioprio = IOPRIO_PRIO_VALUE(new_ioclass, new_iodata);

    int pid = getpid();
    int old_ioprio = syscall(__NR_ioprio_get, IOPRIO_WHO_PROCESS, pid);
    if (old_ioprio == new_ioprio)
        return true;

    int ret = syscall(__NR_ioprio_set, IOPRIO_WHO_PROCESS, pid, new_ioprio);

    if (-1 == ret && EPERM == errno && IOPRIO_CLASS_BE != new_ioclass)
    {
        new_iodata = (new_ioclass == IOPRIO_CLASS_RT) ? 0 : 7;
        new_ioprio = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, new_iodata);
        ret = syscall(__NR_ioprio_set, IOPRIO_WHO_PROCESS, pid, new_ioprio);
    }

    return 0 == ret;
}

#else

bool myth_ioprio(int) { return true; }

#endif

bool myth_FileIsDVD(const QString &filename)
{
    if ((filename.toLower().startsWith("dvd:")) ||
        (filename.toLower().endsWith(".iso")) ||
        (filename.toLower().endsWith(".img")) ||
        (QDir(filename + "/VIDEO_TS").exists()))
        return true;

    if (filename.toLower().startsWith("myth://"))
    {
        QString tmpFile = filename + "/VIDEO_TS";
        if (RemoteFile::Exists(tmpFile))
            return true;
    }

    return false;
}

bool myth_FileIsBD(const QString &filename)
{
    if ((filename.toLower().startsWith("bd:")) ||
        (QDir(filename + "/BDMV").exists()))
        return true;

    if (filename.toLower().startsWith("myth://"))
    {
        QString tmpFile = filename + "/BDMV";
        if (RemoteFile::Exists(tmpFile))
            return true;
    }

    return false;
}

bool RemoveDirectory(QDir &aDir)
{
    if (!aDir.exists())//QDir::NoDotAndDotDot
        return false;

    QFileInfoList entries = aDir.entryInfoList(QDir::NoDotAndDotDot | 
                                               QDir::Dirs | QDir::Files);
    int count = entries.size();
    bool has_err = false;

    for (int idx = 0; idx < count && !has_err; idx++)
    {
        QFileInfo entryInfo = entries[idx];
        QString path = entryInfo.absoluteFilePath();
        if (entryInfo.isDir())
        {
            QDir dir(path);
            has_err = RemoveDirectory(dir);
        }
        else
        {
            QFile file(path);
            if (!file.remove())
                has_err = true;
        }
    }

    if (!has_err && !aDir.rmdir(aDir.absolutePath()))
        has_err = true;

    return(has_err);
}

MPUBLIC QString &ShellEscape(QString &string)
{
    if (string.contains("\""))
        string = string.replace("\"", "\\\"");

    if (string.contains(" "))
    {
        string.prepend("\"");
        string.append("\"");
    }

    return string;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
