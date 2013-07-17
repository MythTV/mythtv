
#include "mythmiscutil.h"

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
#include <QReadWriteLock>
#include <QNetworkProxy>
#include <QStringList>
#include <QUdpSocket>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QUrl>

// Myth headers
#include "mythcorecontext.h"
#include "exitcodes.h"
#include "mythlogging.h"
#include "mythsocket.h"
#include "mythcoreutil.h"
#include "mythsystemlegacy.h"

#include "mythconfig.h" // for CONFIG_DARWIN

/** \fn getUptime(time_t&)
 *  \brief Returns uptime statistics.
 *  \return true if successful, false otherwise.
 */
bool getUptime(time_t &uptime)
{
#ifdef __linux__
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, "sysinfo() error");
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
        LOG(VB_GENERAL, LOG_ERR, "sysctl() error");
        return false;
    }
    else
        uptime = time(NULL) - bootTime.tv_sec;
#elif defined(USING_MINGW)
    uptime = ::GetTickCount() / 1000;
#else
    // Hmmm. Not Linux, not FreeBSD or Darwin. What else is there :-)
    LOG(VB_GENERAL, LOG_NOTICE, "Unknown platform. How do I get the uptime?");
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
        LOG(VB_GENERAL, LOG_ERR,
            "getMemStats(): Error, sysinfo() call failed.");
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
        LOG(VB_GENERAL, LOG_ERR, "getMemStats(): Error, "
            "failed to get virtual memory statistics.");
        return false;
    }

    pageSize >>= 10;  // This gives usages in KB
    totalMB = (s.active_count + s.inactive_count +
               s.wire_count + s.free_count) * pageSize / 1024;
    freeMB  = s.free_count * pageSize / 1024;


    // This is a real hack. I have not found a way to ask the kernel how much
    // swap it is using, and the dynamic_pager daemon doesn't even seem to be
    // able to report what filesystem it is using for the swapfiles. So, we do:
    int64_t total, used, free;
    free = getDiskSpace("/private/var/vm", total, used);
    totalVM = (int)(total >> 10);
    freeVM = (int)(free >> 10);

#else
    LOG(VB_GENERAL, LOG_NOTICE, "getMemStats(): Unknown platform. "
        "How do I get the memory stats?");
    return false;
#endif

    return true;
}

/**
 * \brief Guess whether a string is UTF-8
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
 *
 * Some unixes don't like the -t argument. To make sure a ping failure
 * is actually caused by a defunct server, we might have to do a ping
 * without the -t, which will cause a long timeout.
 */
bool ping(const QString &host, int timeout)
{
#ifdef USING_MINGW
    QString cmd = QString("%systemroot%\\system32\\ping.exe -i %1 -n 1 %2>NUL")
                  .arg(timeout).arg(host);

    if (myth_system(cmd, kMSDontBlockInputDevs | kMSDontDisableDrawing |
                         kMSProcessEvents) != GENERIC_EXIT_OK)
        return false;
#else
    QString cmd = QString("ping -t %1 -c 1  %2  >/dev/null 2>&1")
                  .arg(timeout).arg(host);

    if (myth_system(cmd, kMSDontBlockInputDevs | kMSDontDisableDrawing |
                         kMSProcessEvents) != GENERIC_EXIT_OK)
    {
        // ping command may not like -t argument, or the host might not
        // be listening. Try to narrow down with a quick ping to localhost:

        cmd = "ping -t 1 -c 1 localhost >/dev/null 2>&1";

        if (myth_system(cmd, kMSDontBlockInputDevs | kMSDontDisableDrawing |
                             kMSProcessEvents) != GENERIC_EXIT_OK)
        {
            // Assume -t is bad - do a ping that might cause a timeout:
            cmd = QString("ping -c 1 %1 >/dev/null 2>&1").arg(host);

            if (myth_system(cmd, kMSDontBlockInputDevs | kMSDontDisableDrawing |
                                 kMSProcessEvents) != GENERIC_EXIT_OK)
                return false;  // it failed with or without the -t

            return true;
        }
        else  // Pinging localhost worked, so targeted host wasn't listening
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

    bool connected = s->ConnectToHost(host, port);
    s->DecrRef();

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
        odst = dst.open(QIODevice::Unbuffered |
                        QIODevice::WriteOnly  |
                        QIODevice::Truncate);

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
            LOG(VB_GENERAL, LOG_ERR, "read error");
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
                LOG(VB_GENERAL, LOG_ERR, "write error");
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
    QByteArray safe_name_template = name_template.toLatin1();
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
        LOG(VB_GENERAL, LOG_ERR, QString("createTempFile(%1), Error ")
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
bool makeFileAccessible(QString filename)
{
    QByteArray fname = filename.toLatin1();
    int ret = chmod(fname.constData(), 0666);
    if (ret == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unable to change permissions on file. (%1)").arg(filename));
        return false;
    }
    return true;
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
        LOG(VB_GENERAL, LOG_ERR, "Read from stdin failed");
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
    LOG(VB_GENERAL, LOG_DEBUG,
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
            LOG(VB_GENERAL, LOG_DEBUG, QString("    inter%1: %2")
                    .arg(i).arg((*intermediaries)[i]));
        }
    }

    LOG(VB_GENERAL, LOG_DEBUG,
            QString("getSymlinkTarget() -> '%1'")
            .arg((!fi.isSymLink()) ? cur_file : QString::null));
#endif

    return (!fi.isSymLink()) ? cur_file : QString::null;
}

bool IsMACAddress(QString MAC)
{
    QStringList tokens = MAC.split(':');
    if (tokens.size() != 6)
    {
        LOG(VB_NETWORK, LOG_ERR,
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
            LOG(VB_NETWORK, LOG_ERR,
                QString("IsMACAddress(%1) = false, part #%2 is empty.")
                    .arg(MAC).arg(y));
            return false;
        }

        value = tokens[y].toInt(&ok, 16);
        if (!ok)
        {
            LOG(VB_NETWORK, LOG_ERR,
                QString("IsMACAddress(%1) = false, unable to "
                        "convert part '%2' to integer.")
                    .arg(MAC).arg(tokens[y]));
            return false;
        }

        if (value > 255)
        {
            LOG(VB_NETWORK, LOG_ERR,
                QString("IsMACAddress(%1) = false, part #%2 "
                        "evaluates to %3 which is higher than 255.")
                    .arg(MAC).arg(y).arg(value));
            return false;
        }
    }

    LOG(VB_NETWORK, LOG_DEBUG, QString("IsMACAddress(%1) = true").arg(MAC));
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
        LOG(VB_GENERAL, LOG_ERR,
            "Error: Unable to open selected file, missing read permissions?");
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
        LOG(VB_GENERAL, LOG_ERR,
                QString( "WakeOnLan(%1): Incorrect MAC length").arg(MAC));
        return false;
    }

    for (y = 0; y < 6; y++)
    {
        macaddr[y] = tokens[y].toInt(&ok, 16);

        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString( "WakeOnLan(%1): Invalid MAC address").arg(MAC));
            return false;
        }
    }

    for (x = 0; x < 16; x++)
        for (y = 0; y < 6; y++)
            msg[msglen++] = macaddr[y];

    LOG(VB_NETWORK, LOG_INFO,
            QString("WakeOnLan(): Sending WOL packet to %1").arg(MAC));

    QUdpSocket udp_socket;
    return udp_socket.writeDatagram(
        msg, msglen, QHostAddress::Broadcast, 32767) == msglen;
}

bool IsPulseAudioRunning(void)
{
#ifdef USING_MINGW
    return false;
#else

#if CONFIG_DARWIN || (__FreeBSD__) || defined(__OpenBSD__)
    const char *command = "ps -ax | grep -i pulseaudio | grep -v grep > /dev/null";
#else
    const char *command = "ps ch -C pulseaudio -o pid > /dev/null";
#endif
    // Do NOT use kMSProcessEvents here, it will cause deadlock
    uint res = myth_system(command, kMSDontBlockInputDevs |
                                    kMSDontDisableDrawing);
    return (res == GENERIC_EXIT_OK);
#endif // USING_MINGW
}

bool myth_nice(int val)
{
    errno = 0;
    int ret = nice(val);

    if ((-1 == ret) && (0 != errno) && (val >= 0))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to nice process" + ENO);
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

bool MythRemoveDirectory(QDir &aDir)
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
            has_err = MythRemoveDirectory(dir);
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

/**
 * \brief Get network proxy settings from OS, and use for [Q]Http[Comms]
 *
 * The HTTP_PROXY environment var. is parsed for values like; "proxy-host",
 * "proxy-host:8080", "http://host:8080" and "http"//user:password@host:1080",
 * and that is used for any Qt-based Http fetches.
 * We also test connectivity here with ping and telnet, and warn if it fails.
 *
 * If there is was no env. var, we use Qt to get proxy settings from the OS,
 * and search through them for a proxy server we can connect to.
 */
void setHttpProxy(void)
{
    QString       LOC = "setHttpProxy() - ";
    QNetworkProxy p;


    // Set http proxy for the application if specified in environment variable
    QString var(getenv("http_proxy"));
    if (var.isEmpty())
        var = getenv("HTTP_PROXY");  // Sadly, some OS envs are case sensitive
    if (var.length())
    {
        if (!var.startsWith("http://"))   // i.e. just a host name
            var.prepend("http://");

        QUrl    url  = QUrl(var, QUrl::TolerantMode);
        QString host = url.host();
        int     port = url.port();

        if (port == -1)   // Parsing error
        {
            port = 0;   // The default when creating a QNetworkProxy

            if (telnet(host, 1080))  // Socks?
                port = 1080;
            if (telnet(host, 3128))  // Squid
                port = 3128;
            if (telnet(host, 8080))  // MS ISA
                port = 8080;

            LOG(VB_NETWORK, LOG_INFO, LOC +
                QString("assuming port %1 on host %2") .arg(port).arg(host));
            url.setPort(port);
        }
        else if (!ping(host, 1))
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("cannot locate host %1").arg(host) +
                "\n\t\t\tPlease check HTTP_PROXY environment variable!");
        else if (!telnet(host,port))
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("%1:%2 - cannot connect!").arg(host).arg(port) +
                "\n\t\t\tPlease check HTTP_PROXY environment variable!");

#if 0
        LOG(VB_NETWORK, LOG_DEBUG, LOC + QString("using http://%1:%2@%3:%4")
                .arg(url.userName()).arg(url.password())
                .arg(host).arg(port));
#endif
        p = QNetworkProxy(QNetworkProxy::HttpCachingProxy,
                          host, port, url.userName(), url.password());
        QNetworkProxy::setApplicationProxy(p);
        return;
    }

    LOG(VB_NETWORK, LOG_DEBUG, LOC + "no HTTP_PROXY environment var.");

    // Use Qt to look for user proxy settings stored by the OS or browser:

    QList<QNetworkProxy> proxies;
    QNetworkProxyQuery   query(QUrl("http://www.mythtv.org"));

    proxies = QNetworkProxyFactory::systemProxyForQuery(query);

    Q_FOREACH (p, proxies)
    {
        QString host = p.hostName();
        int     port = p.port();

        if (p.type() == QNetworkProxy::NoProxy)
            continue;

        if (!telnet(host, port))
        {
            LOG(VB_NETWORK, LOG_ERR, LOC +
                "failed to contact proxy host " + host);
            continue;
        }

        LOG(VB_NETWORK, LOG_INFO, LOC + QString("using proxy host %1:%2")
                            .arg(host).arg(port));
        QNetworkProxy::setApplicationProxy(p);

        // Allow sub-commands to use this proxy
        // via myth_system(command), by setting HTTP_PROXY
        QString url;

        if (p.user().length())
            url = "http://%1:%2@%3:%4",
            url = url.arg(p.user()).arg(p.password());
        else
            url = "http://%1:%2";

        url = url.arg(p.hostName()).arg(p.port());
        setenv("HTTP_PROXY", url.toLatin1(), 1);
        setenv("http_proxy", url.toLatin1(), 0);

        return;
    }

    LOG(VB_NETWORK, LOG_ERR, LOC + "failed to find a network proxy");
}

void wrapList(QStringList &list, int width)
{
    int i;

    // if this is triggered, something has gone seriously wrong
    // the result won't really be usable, but at least it won't crash
    width = max(width, 5);

    for(i = 0; i < list.size(); i++)
    {
        QString string = list.at(i);

        if( string.size() <= width )
            continue;

        QString left   = string.left(width);
        bool inserted  = false;

        while( !inserted && !left.endsWith(" " ))
        {
            if( string.mid(left.size(), 1) == " " )
            {
                list.replace(i, left);
                list.insert(i+1, string.mid(left.size()).trimmed());
                inserted = true;
            }
            else
            {
                left.chop(1);
                if( !left.contains(" ") )
                {
                    // Line is too long, just hyphenate it
                    list.replace(i, left + "-");
                    list.insert(i+1, string.mid(left.size()));
                    inserted = true;
                }
            }
        }

        if( !inserted )
        {
            left.chop(1);
            list.replace(i, left);
            list.insert(i+1, string.mid(left.size()).trimmed());
        }
    }
}

QString xml_indent(uint level)
{
    static QReadWriteLock rw_lock;
    static QMap<uint,QString> cache;

    rw_lock.lockForRead();
    QMap<uint,QString>::const_iterator it = cache.find(level);
    if (it != cache.end())
    {
        QString tmp = *it;
        rw_lock.unlock();
        return tmp;
    }
    rw_lock.unlock();

    QString ret = "";
    for (uint i = 0; i < level; i++)
        ret += "    ";

    rw_lock.lockForWrite();
    cache[level] = ret;
    rw_lock.unlock();

    return ret;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
