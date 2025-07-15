#ifdef _WIN32
    #include <sys/stat.h>
#endif

#include "mythmiscutil.h"

// C++ headers
#include <array>
#include <cerrno>
#include <cstdlib>
#include <iostream>

// POSIX
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>

// System specific C headers
#include "compat.h"
#include <QtGlobal>

#ifdef __linux__
#include <sys/vfs.h>
#include <sys/sysinfo.h>
#include <sys/stat.h> // for umask, chmod
#endif

#ifdef Q_OS_DARWIN
#include <mach/mach.h>
#endif

#ifdef BSD
#include <sys/mount.h>  // for struct statfs
#include <sys/sysctl.h>
#include <sys/stat.h> // for umask, chmod
#endif

// Qt headers
#include <QReadWriteLock>
#include <QNetworkProxy>
#include <QStringList>
#include <QDataStream>
#include <QUdpSocket>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QHostAddress>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>

// Myth headers
#include "mythcorecontext.h"
#include "exitcodes.h"
#include "mythlogging.h"
#include "mythsocket.h"
#include "filesysteminfo.h"
#include "mythsystemlegacy.h"


/**
 *  \brief Returns uptime statistics.
 *  \return true if successful, false otherwise.
 */
bool getUptime(std::chrono::seconds &uptime)
{
#ifdef __linux__
    struct sysinfo sinfo {};
    if (sysinfo(&sinfo) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, "sysinfo() error");
        return false;
    }
    uptime = std::chrono::seconds(sinfo.uptime);

#elif defined(__FreeBSD__) || defined(Q_OS_DARWIN)

    std::array<int,2> mib { CTL_KERN, KERN_BOOTTIME };
    struct timeval bootTime;
    size_t         len;

    // Uptime is calculated. Get this machine's boot time
    // and subtract it from the current machine time
    len    = sizeof(bootTime);
    if (sysctl(mib.data(), 2, &bootTime, &len, nullptr, 0) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, "sysctl() error");
        return false;
    }
    uptime = std::chrono::seconds(time(nullptr) - bootTime.tv_sec);
#elif defined(_WIN32)
    uptime = std::chrono::seconds(::GetTickCount() / 1000);
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
 *  \todo keep values as B not MiB, int64_t (or size_t?)
 *  \return true if it succeeds, false otherwise.
 */
bool getMemStats([[maybe_unused]] int &totalMB,
                 [[maybe_unused]] int &freeMB,
                 [[maybe_unused]] int &totalVM,
                 [[maybe_unused]] int &freeVM)
{
#ifdef __linux__
    static constexpr size_t MB { 1024LL * 1024 };
    struct sysinfo sinfo {};
    if (sysinfo(&sinfo) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "getMemStats(): Error, sysinfo() call failed.");
        return false;
    }

    totalMB = (int)((sinfo.totalram  * sinfo.mem_unit)/MB);
    freeMB  = (int)((sinfo.freeram   * sinfo.mem_unit)/MB);
    totalVM = (int)((sinfo.totalswap * sinfo.mem_unit)/MB);
    freeVM  = (int)((sinfo.freeswap  * sinfo.mem_unit)/MB);
    return true;
#elif defined(Q_OS_DARWIN)
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
    {
    auto fsInfo = FileSystemInfo(QString(), "/private/var/vm");
    totalVM = (int)(fsInfo.getTotalSpace() >> 10);
    freeVM  = (int)(fsInfo.getFreeSpace()  >> 10);
    }
    return true;
#else
    return false;
#endif
}

/** \fn getLoadAvgs()
 *  \brief Returns the system load averages.
 *  \return A std::array<double,3> containing the system load
 *          averages.  If the system call fails or is unsupported,
 *          returns array containing all -1.
 */
loadArray getLoadAvgs (void)
{
#if !defined(_WIN32) && !defined(Q_OS_ANDROID)
    loadArray loads {};
    if (getloadavg(loads.data(), loads.size()) != -1)
        return loads;
#endif
    return {-1, -1, -1};
}

/**
 * \brief Can we ping host within timeout seconds?
 *
 * Different operating systems use different parameters to specify a
 * timeout to the ping command.  FreeBSD and derivatives use '-t';
 * Linux and derivatives use '-w'.  Using the right parameter also
 * eliminates the need for the old behavior of falling back to pinging
 * the localhost with and without a timeout, in order to characterize
 * whether the right parameter was used in the first place.
 *
 * \note If ping fails to timeout on another supported platform,
 * determine what parameter that platform's ping requires to specify
 * the timeout and add another case to the \#ifdef statement.
 */
bool ping(const QString &host, std::chrono::milliseconds timeout)
{
#ifdef _WIN32
    QString cmd = QString("%systemroot%\\system32\\ping.exe -w %1 -n 1 %2>NUL")
                  .arg(timeout.count()) .arg(host);

    return myth_system(cmd, kMSDontBlockInputDevs | kMSDontDisableDrawing |
                         kMSProcessEvents) == GENERIC_EXIT_OK;
#else
    QString addrstr =
        MythCoreContext::resolveAddress(host, MythCoreContext::ResolveAny, true);
    QHostAddress addr = QHostAddress(addrstr);
#if defined(__FreeBSD__) || defined(Q_OS_DARWIN)
    QString timeoutparam("-t");
#else
    // Linux, NetBSD, OpenBSD
    QString timeoutparam("-w");
#endif // UNIX-like
    QString pingcmd =
        addr.protocol() == QAbstractSocket::IPv6Protocol ? "ping6" : "ping";
    QString cmd = QString("%1 %2 %3 -c 1  %4  >/dev/null 2>&1")
                  .arg(pingcmd, timeoutparam,
                       QString::number(duration_cast<std::chrono::seconds>(timeout).count()),
                       host);

    return myth_system(cmd, kMSDontBlockInputDevs | kMSDontDisableDrawing |
                         kMSProcessEvents) == GENERIC_EXIT_OK;
#endif // _WIN32
}

/**
 * \brief Can we talk to port on host?
 */
bool telnet(const QString &host, int port)
{
    auto *s = new MythSocket();

    bool connected = s->ConnectToHost(host, port);
    s->DecrRef();

    return connected;
}

/**
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
long long MythFile::copy(QFile &dst, QFile &src, uint block_size)
{
    uint buflen = (block_size < 1024) ? (16 * 1024) : block_size;
    char *buf = new char[buflen];
    bool odst = false;
    bool osrc = false;

    if (!buf)
        return -1LL;

    if (!dst.isWritable() && !dst.isOpen())
    {
        odst = dst.open(QIODevice::Unbuffered |
                        QIODevice::WriteOnly  |
                        QIODevice::Truncate);
    }

    if (!src.isReadable() && !src.isOpen())
        osrc = src.open(QIODevice::Unbuffered|QIODevice::ReadOnly);

    bool ok = dst.isWritable() && src.isReadable();
    long long total_bytes = 0LL;
    while (ok)
    {
        long long off = 0;
        long long rlen = src.read(buf, buflen);
        if (rlen<0)
        {
            LOG(VB_GENERAL, LOG_ERR, "read error");
            ok = false;
            break;
        }
        if (rlen==0)
            break;

        total_bytes += rlen;

        while ((rlen-off>0) && ok)
        {
            long long wlen = dst.write(buf + off, rlen - off);
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

#ifdef _WIN32
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
bool makeFileAccessible(const QString& filename)
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
    std::cout << tmp.constData();

    tmp = def.toLocal8Bit();
    if (!def.isEmpty())
        std::cout << " [" << tmp.constData() << "]  ";
    else
        std::cout << "  ";

    if (!isatty(fileno(stdin)) || !isatty(fileno(stdout)))
    {
        std::cout << std::endl << "[console is not interactive, using default '"
             << tmp.constData() << "']" << std::endl;
        return def;
    }

    QTextStream stream(stdin);
    QString     qresponse = stream.readLine();

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
    bool ok = false;
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

    QString   link;
    QString   cur_file = start_file;
    QFileInfo fi(cur_file);

    if (intermediaries)
    {
        intermediaries->clear();
        intermediaries->push_back(start_file);
    }

    for (uint i = 0; (i <= maxLinks) && fi.isSymLink() &&
             !(link = fi.symLinkTarget()).isEmpty(); i++)
    {
        cur_file = (link[0] == '/') ?
            link : // absolute link
            fi.absoluteDir().absolutePath() + "/" + link; // relative link

        if (intermediaries && !intermediaries->contains(cur_file))
            intermediaries->push_back(cur_file);

        fi = QFileInfo(cur_file);
    }

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
            .arg((!fi.isSymLink()) ? cur_file : QString()));
#endif

    return (!fi.isSymLink()) ? cur_file : QString();
}

bool IsMACAddress(const QString& MAC)
{
    QStringList tokens = MAC.split(':');
    if (tokens.size() != 6)
    {
        LOG(VB_NETWORK, LOG_ERR,
            QString("IsMACAddress(%1) = false, doesn't have 6 parts").arg(MAC));
        return false;
    }

    for (int y = 0; y < 6; y++)
    {
        if (tokens[y].isEmpty())
        {
            LOG(VB_NETWORK, LOG_ERR,
                QString("IsMACAddress(%1) = false, part #%2 is empty.")
                    .arg(MAC).arg(y));
            return false;
        }

        bool ok = false;
        int value = tokens[y].toInt(&ok, 16);
        if (!ok)
        {
            LOG(VB_NETWORK, LOG_ERR,
                QString("IsMACAddress(%1) = false, unable to "
                        "convert part '%2' to integer.")
                    .arg(MAC, tokens[y]));
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

QString FileHash(const QString& filename)
{
    QFile file(filename);
    QFileInfo fileinfo(file);
    qint64 initialsize = fileinfo.size();
    quint64 hash = 0;

    if (initialsize == 0)
        return {"NULL"};

    if (file.open(QIODevice::ReadOnly))
        hash = initialsize;
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Error: Unable to open selected file, missing read permissions?");
        return {"NULL"};
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

bool WakeOnLAN(const QString& MAC)
{
    std::vector<char> msg(6, static_cast<char>(0xFF));
    std::array<char,6> macaddr {};
    QStringList tokens = MAC.split(':');

    if (tokens.size() != 6)
    {
        LOG(VB_GENERAL, LOG_ERR,
                QString( "WakeOnLan(%1): Incorrect MAC length").arg(MAC));
        return false;
    }

    for (int y = 0; y < 6; y++)
    {
        bool ok = false;
        macaddr[y] = tokens[y].toInt(&ok, 16);

        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString( "WakeOnLan(%1): Invalid MAC address").arg(MAC));
            return false;
        }
    }

    msg.reserve(1024);
    for (int x = 0; x < 16; x++)
        msg.insert(msg.end(), macaddr.cbegin(), macaddr.cend());

    LOG(VB_NETWORK, LOG_INFO,
            QString("WakeOnLan(): Sending WOL packet to %1").arg(MAC));

    QUdpSocket udp_socket;
    qlonglong msglen = msg.size();
    return udp_socket.writeDatagram(
        msg.data(), msglen, QHostAddress::Broadcast, 32767) == msglen;
}

// Wake up either by command or by MAC address
// return true = success
bool MythWakeup(const QString &wakeUpCommand, uint flags, std::chrono::seconds timeout)
{
    if (!IsMACAddress(wakeUpCommand))
        return myth_system(wakeUpCommand, flags, timeout) == 0U;

    return WakeOnLAN(wakeUpCommand);
}

bool IsPulseAudioRunning(void)
{
#ifdef _WIN32
    return false;
#else

#if defined(Q_OS_DARWIN) || defined(__FreeBSD__) || defined(__OpenBSD__)
    const char *command = "ps -ax | grep -i pulseaudio | grep -v grep > /dev/null";
#else
    const char *command = "ps ch -C pulseaudio -o pid > /dev/null";
#endif
    // Do NOT use kMSProcessEvents here, it will cause deadlock
    uint res = myth_system(command, kMSDontBlockInputDevs |
                                    kMSDontDisableDrawing);
    return (res == GENERIC_EXIT_OK);
#endif // _WIN32
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

#include <cstdio>
#include <getopt.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#if __has_include(<linux/ioprio.h>)
// Starting with kernel 6.5.0, the following include uses the C++
// reserved keyword "class" as a variable name. Fortunately we can
// redefine it without any ill effects.
#define class class2
#include <linux/ioprio.h>
#undef class
#else
static constexpr int8_t IOPRIO_BITS        { 16 };
static constexpr int8_t IOPRIO_CLASS_SHIFT { 13 };
static constexpr int    IOPRIO_PRIO_MASK   { (1UL << IOPRIO_CLASS_SHIFT) - 1 };
static constexpr int IOPRIO_PRIO_CLASS(int mask)
    { return mask >> IOPRIO_CLASS_SHIFT; };
static constexpr int IOPRIO_PRIO_DATA(int mask)
    { return mask & IOPRIO_PRIO_MASK; };
static constexpr int IOPRIO_PRIO_VALUE(int pclass, int data)
    { return (pclass << IOPRIO_CLASS_SHIFT) | data; };

enum { IOPRIO_CLASS_NONE,IOPRIO_CLASS_RT,IOPRIO_CLASS_BE,IOPRIO_CLASS_IDLE, };
enum { IOPRIO_WHO_PROCESS = 1, IOPRIO_WHO_PGRP, IOPRIO_WHO_USER, };
#endif // has_include(<linux/ioprio.h>)

bool myth_ioprio(int val)
{
    int new_ioclass {IOPRIO_CLASS_BE};
    if (val < 0)
        new_ioclass = IOPRIO_CLASS_RT;
    else if (val > 7)
        new_ioclass = IOPRIO_CLASS_IDLE;
    int new_iodata = (new_ioclass == IOPRIO_CLASS_BE) ? val : 0;
    int new_ioprio = IOPRIO_PRIO_VALUE(new_ioclass, new_iodata);

    int pid = getpid();
    int old_ioprio = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, pid);
    if (old_ioprio == new_ioprio)
        return true;

    int ret = syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, pid, new_ioprio);

    if (-1 == ret && EPERM == errno && IOPRIO_CLASS_BE != new_ioclass)
    {
        new_iodata = (new_ioclass == IOPRIO_CLASS_RT) ? 0 : 7;
        new_ioprio = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, new_iodata);
        ret = syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, pid, new_ioprio);
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
        const QFileInfo& entryInfo = entries[idx];
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
 * "proxy-host:8080", "http://host:8080" and "http"//user:password\@host:1080",
 * and that is used for any Qt-based Http fetches.
 * We also test connectivity here with ping and telnet, and warn if it fails.
 *
 * If there is was no env. var, we use Qt to get proxy settings from the OS,
 * and search through them for a proxy server we can connect to.
 */
void setHttpProxy(void)
{
    QString       LOC = "setHttpProxy() - ";

    // Set http proxy for the application if specified in environment variable
    QString var(qEnvironmentVariable("http_proxy"));
    if (var.isEmpty())
        var = qEnvironmentVariable("HTTP_PROXY");  // Sadly, some OS envs are case sensitive
    if (!var.isEmpty())
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
        else if (!ping(host, 1s))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("cannot locate host %1").arg(host) +
                "\n\t\t\tPlease check HTTP_PROXY environment variable!");
        }
        else if (!telnet(host,port))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("%1:%2 - cannot connect!").arg(host).arg(port) +
                "\n\t\t\tPlease check HTTP_PROXY environment variable!");
        }

#if 0
        LOG(VB_NETWORK, LOG_DEBUG, LOC + QString("using http://%1:%2@%3:%4")
                .arg(url.userName()).arg(url.password())
                .arg(host).arg(port));
#endif
        QNetworkProxy p =
            QNetworkProxy(QNetworkProxy::HttpCachingProxy,
                          host, port, url.userName(), url.password());
        QNetworkProxy::setApplicationProxy(p);
        return;
    }

    LOG(VB_NETWORK, LOG_DEBUG, LOC + "no HTTP_PROXY environment var.");

    // Use Qt to look for user proxy settings stored by the OS or browser:

    QList<QNetworkProxy> proxies;
    QNetworkProxyQuery   query(QUrl("http://www.mythtv.org"));

    proxies = QNetworkProxyFactory::systemProxyForQuery(query);

    for (const auto& p : std::as_const(proxies))
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

        if (!p.user().isEmpty())
        {
            url = "http://%1:%2@%3:%4",
            url = url.arg(p.user(), p.password());
        }
        else
        {
            url = "http://%1:%2";
        }

        url = url.arg(p.hostName()).arg(p.port());
        setenv("HTTP_PROXY", url.toLatin1(), 1);
        setenv("http_proxy", url.toLatin1(), 0);

        return;
    }

    LOG(VB_NETWORK, LOG_ERR, LOC + "failed to find a network proxy");
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
