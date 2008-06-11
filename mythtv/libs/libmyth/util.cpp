// C++ headers
#include <iostream>

#include "mythcontext.h"

using namespace std;

// C headers
#include <cerrno>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

// System specific C headers
#include "compat.h"
#ifdef USING_MINGW
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/param.h>
#else
# include <sys/types.h>
# include <sys/wait.h>
# include <sys/stat.h>
# ifdef linux
#   include <sys/vfs.h>
#   include <sys/statvfs.h>
#   include <sys/sysinfo.h>
# else
#   include <sys/param.h>
#   ifdef __FreeBSD__ 
#     include <sys/mount.h> 
#   endif
#   ifdef CONFIG_CYGWIN
#     include <sys/statfs.h>
#   else // if !CONFIG_CYGWIN
#     include <sys/sysctl.h>
#   endif // !CONFIG_CYGWIN
# endif
#endif //MINGW

// Qt headers
#include <qapplication.h>
#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qfont.h>
#include <qfile.h>
#include <Q3CString>
#include <Q3DeepCopy>

// Myth headers
#include "mythconfig.h"
#include "exitcodes.h"
#include "util.h"
#include "util-x11.h"
#include "mythmediamonitor.h"

#ifdef CONFIG_DARWIN
#include <mach/mach.h> 
#include <sys/mount.h>  // for struct statfs
#endif

#ifdef USE_LIRC
#include "lircevent.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenuevent.h"
#endif

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

int calc_utc_offset(void)
{
    QDateTime loc = QDateTime::currentDateTime(Qt::LocalTime);
    QDateTime utc = QDateTime::currentDateTime(Qt::UTC);

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

/** \fn encodeLongLong(QStringList&,long long)
 *  \brief Encodes a long for streaming in the MythTV protocol.
 *
 *   We need this for Qt3.1 compatibility, since it will not
 *   print or read a 64 bit number directly.
 *   We encode the long long as strings representing two signed
 *   32 bit integers.
 *
 *  \sa decodeLongLong(QStringList&,uint)
 *      decodeLongLong(QStringList&,QStringList::const_iterator&)
 */
void encodeLongLong(QStringList &list, long long num)
{
    list << QString::number((int)(num >> 32));
    list << QString::number((int)(num & 0xffffffffLL));
}

/** \fn decodeLongLong(QStringList&,uint)
 *  \brief Inefficiently decodes a long encoded for streaming in the MythTV protocol.
 *
 *   We need this for Qt3.1 compatibility, since it will not
 *   print or read a 64 bit number directly.
 *
 *   The long long is represented as two signed 32 bit integers.
 *
 *   Note: This decode performs two O(n) linear searches of the list,
 *         The iterator decode function is much more efficient.
 *
 *  \param list   List to search for offset and offset+1 in.
 *  \param offset Offset in list where to find first 32 bits of
 *                long long.
 *  \sa encodeLongLong(QStringList&,long long),
 *      decodeLongLong(QStringList&,QStringList::const_iterator&)
 */
long long decodeLongLong(QStringList &list, uint offset)
{
    long long retval = 0;
    if (offset >= (uint)list.size())
    {
        VERBOSE(VB_IMPORTANT,
                "decodeLongLong() called with offset >= list size.");
        return retval;
    }

    int l1 = list[offset].toInt();
    int l2 = list[offset + 1].toInt();

    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
}

/** \fn decodeLongLong(QStringList&,QStringList::const_iterator&)
 *  \brief Decodes a long encoded for streaming in the MythTV protocol.
 *
 *   We need this for Qt3.1 compatibility, since it will not
 *   print or read a 64 bit number directly.
 *
 *   The long long is represented as two signed 32 bit integers.
 *
 *  \param list   List to search for offset and offset+1 in.
 *  \param it     Iterator pointing to first 32 bits of long long.
 *  \sa encodeLongLong(QStringList&,long long),
 *      decodeLongLong(QStringList&,uint)
 */
long long decodeLongLong(QStringList &list, QStringList::const_iterator &it)
{
    (void)list;

    long long retval = 0;

    bool ok = true;
    int l1=0, l2=0;

    if (it == list.end())
        ok = false;
    else
        l1 = (*(it++)).toInt();

    if (it == list.end())
        ok = false;
    else
        l2 = (*(it++)).toInt();
 
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT,
                "decodeLongLong() called with the iterator too close "
                "to the end of the list.");
        return 0;
    }

    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
} 

/** \fn blendColors(QRgb source, QRgb add, int alpha)
 *  \brief Inefficient alpha blending function.
 */
QRgb blendColors(QRgb source, QRgb add, int alpha)
{
    int sred = qRed(source);
    int sgreen = qGreen(source);
    int sblue = qBlue(source);

    int tmp1 = (qRed(add) - sred) * alpha;
    int tmp2 = sred + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    sred = tmp2 & 0xff;

    tmp1 = (qGreen(add) - sgreen) * alpha;
    tmp2 = sgreen + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    sgreen = tmp2 & 0xff;

    tmp1 = (qBlue(add) - sblue) * alpha;
    tmp2 = sblue + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    sblue = tmp2 & 0xff;

    return qRgb(sred, sgreen, sblue);
}

/** \fn myth_system(const QString&, int)
 *  \brief Runs a system command inside the /bin/sh shell.
 *  
 *  Note: Returns GENERIC_EXIT_NOT_OK if it can not execute the command.
 *  \return Exit value from command as an unsigned int in range [0,255].
 */
uint myth_system(const QString &command, int flags)
{
    (void)flags; /* Kill warning */

    bool ready_to_lock = gContext && gContext->GetMainWindow();

    (void)ready_to_lock; /* Kill warning */
#ifdef USE_LIRC
    bool lirc_lock_flag = !(flags & MYTH_SYSTEM_DONT_BLOCK_LIRC);
    LircEventLock lirc_lock(lirc_lock_flag && ready_to_lock);
#endif

#ifdef USE_JOYSTICK_MENU
    bool joy_lock_flag = !(flags & MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU);
    JoystickMenuEventLock joystick_lock(joy_lock_flag && ready_to_lock);
#endif

#ifndef USING_MINGW
    pid_t child = fork();

    if (child < 0)
    {
        /* Fork failed */
        VERBOSE(VB_IMPORTANT,
                QString("myth_system(): Error, fork() failed because %1")
                .arg(strerror(errno)));
        return GENERIC_EXIT_NOT_OK;
    }
    else if (child == 0)
    {
        /* Child */
        /* Close all open file descriptors except stdout/stderr */
        for (int i = sysconf(_SC_OPEN_MAX) - 1; i > 2; i--)
            close(i);

        /* Attach stdin to /dev/null */
        close(0);
        int fd = open("/dev/null", O_RDONLY);
        dup2(fd, 0);
        if (fd != 0)
            close(fd);

        /* Run command */
        execl("/bin/sh", "sh", "-c", command.toUtf8().constData(), NULL);
        if (errno)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("myth_system(): Error, execl() failed because %1")
                    .arg(strerror(errno)));
        }

        /* Failed to exec */
        _exit(MYTHSYSTEM__EXIT__EXECL_ERROR); // this exit is ok
    }
    else
    {
        /* Parent */
        int status;

        if (flags & MYTH_SYSTEM_DONT_BLOCK_PARENT)
        {
            int res = 0;

            while (res == 0)
            {
                res = waitpid(child, &status, WNOHANG);
                if (res == -1)
                {
                    VERBOSE(VB_IMPORTANT,
                            QString("myth_system(): Error, waitpid() failed because %1")
                            .arg(strerror(errno)));
                    return GENERIC_EXIT_NOT_OK;
                }

                qApp->processEvents();

                if (res > 0)
                    return WEXITSTATUS(status);

                usleep(100000);
            }
        }
        else
        {
            if (waitpid(child, &status, 0) < 0)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("myth_system(): Error, waitpid() failed because %1")
                        .arg(strerror(errno)));
                return GENERIC_EXIT_NOT_OK;
            }
            return WEXITSTATUS(status);
        }
    }

    return GENERIC_EXIT_NOT_OK;
}
#else
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    QString cmd = QString("cmd.exe /c %1").arg(command);
    if (!::CreateProcessA(NULL, cmd.toUtf8().data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        VERBOSE(VB_IMPORTANT,
                QString("myth_system(): Error, CreateProcess() failed because %1")
                .arg(::GetLastError()));
        return MYTHSYSTEM__EXIT__EXECL_ERROR;
    } else {
        if (::WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            VERBOSE(VB_IMPORTANT,
                    QString("myth_system(): Error, WaitForSingleObject() failed because %1")
                    .arg(::GetLastError()));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return GENERIC_EXIT_OK;
    }
    return GENERIC_EXIT_NOT_OK;
}
#endif

/** \fn cutDownString(const QString&, QFont*, uint)
 *  \brief Returns a string based on "text" that fits within "maxwidth" pixels.
 */
QString cutDownString(const QString &text, QFont *testFont, uint maxwidth)
{
    QFontMetrics fm(*testFont);

    uint curFontWidth = fm.width(text);
    if (curFontWidth > maxwidth)
    {
        QString testInfo = "";
        curFontWidth = fm.width(testInfo);
        int tmaxwidth = maxwidth - fm.width("LLL");
        int count = 0;

        while ((int)curFontWidth < tmaxwidth)
        {
            testInfo = text.left(count);
            curFontWidth = fm.width(testInfo);
            count = count + 1;
        }

        testInfo = testInfo + "...";
        return testInfo;
    }

    return text;
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
    
/** \fn stringToLongLong(const QString &str)
 *  \brief Converts QString representing long long to a long long.
 *
 *   This is needed to input 64 bit numbers with Qt 3.1.
 */
long long stringToLongLong(const QString &str)
{
    long long retval = 0;
    if (str != QString::null)
    {
        retval = strtoll(str.ascii(), NULL, 0);
    }
    return retval;
}

/** \fn longLongToString(long long)
 *  \brief Returns QString representation of long long.
 *
 *   This is needed to output 64 bit numbers with Qt 3.1.
 */
QString longLongToString(long long ll)
{
    char str[21];
    snprintf(str, 20, "%lld", ll);
    str[20] = '\0';
    return str;
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

#elif defined(__FreeBSD__) || defined(CONFIG_DARWIN)

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

/** \fn getDiskSpace(const QString&,long long&,long long&)
 *  \brief Returns free space on disk containing file in KiB,
 *          or -1 if it does not succeed.
 *  \param file_on_disk file on the file system we wish to stat.
 */
long long getDiskSpace(const QString &file_on_disk,
                       long long &total, long long &used)
{
    struct statfs statbuf;
    bzero(&statbuf, sizeof(statbuf));
    long long freespace = -1;
    Q3CString cstr = file_on_disk.local8Bit();

    total = used = -1;

    // there are cases where statfs will return 0 (good), but f_blocks and
    // others are invalid and set to 0 (such as when an automounted directory
    // is not mounted but still visible because --ghost was used),
    // so check to make sure we can have a total size > 0
    if ((statfs(cstr, &statbuf) == 0) &&
        (statbuf.f_blocks > 0) &&
        (statbuf.f_bsize > 0))
    {
        total      = statbuf.f_blocks;
        total     *= statbuf.f_bsize;
        total      = total >> 10;

        freespace  = statbuf.f_bavail;
        freespace *= statbuf.f_bsize;
        freespace  = freespace >> 10;

        used       = total - freespace;
    }

    return freespace;
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

#elif defined(CONFIG_DARWIN)
    mach_port_t             mp;
    mach_msg_type_number_t  count, pageSize;
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
#elif defined(CONFIG_DARWIN)
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

#ifdef USING_MINGW
u_short in_cksum(u_short *addr, int len)
{
	register int nleft = len;
	register u_short *w = addr;
	register u_short answer;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while( nleft > 1 )  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if( nleft == 1 ) {
		u_short	u = 0;

		*(u_char *)(&u) = *(u_char *)w ;
		sum += u;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}
#endif

/**
 * \brief Can we ping host within timeout seconds?
 */
bool ping(const QString &host, int timeout)
{
#ifdef USING_MINGW
    VERBOSE(VB_SOCKET, QString("Ping: pinging %1 (%2 seconds max)").arg(host).arg(timeout));
	SOCKET	  rawSocket;
	LPHOSTENT lpHost;
	struct    sockaddr_in saDest;

    #define ICMP_ECHOREPLY	0
    #define ICMP_ECHOREQ	8
    struct IPHDR {
	    u_char  VIHL;			// Version and IHL
	    u_char	TOS;			// Type Of Service
	    short	TotLen;			// Total Length
	    short	ID;				// Identification
	    short	FlagOff;		// Flags and Fragment Offset
	    u_char	TTL;			// Time To Live
	    u_char	Protocol;		// Protocol
	    u_short	Checksum;		// Checksum
	    struct	in_addr iaSrc;	// Internet Address - Source
	    struct	in_addr iaDst;	// Internet Address - Destination
    };
    struct ICMPHDR {
	    u_char	Type;			// Type
	    u_char	Code;			// Code
	    u_short	Checksum;		// Checksum
	    u_short	ID;				// Identification
	    u_short	Seq;			// Sequence
	    char	Data;			// Data
    };

    struct Request {
	    ICMPHDR icmpHdr;
	    DWORD	dwTime;
	    char	cData[32];
    };
    struct Reply {
	    IPHDR	ipHdr;
	    Request	echoRequest;
	    char    cFiller[256];
    };

    if (INVALID_SOCKET == (rawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP))) {
        VERBOSE(VB_SOCKET, "Ping: can't create socket");
    	return false;
    }

	lpHost = gethostbyname(host);
    if (!lpHost) {
        VERBOSE(VB_SOCKET, "Ping: gethostbyname failed");
        closesocket(rawSocket);
		return false;
    }

    saDest.sin_addr.s_addr = *((u_long FAR *) (lpHost->h_addr));
	saDest.sin_family = AF_INET;
	saDest.sin_port = 0;

    Request echoReq;
	echoReq.icmpHdr.Type		= ICMP_ECHOREQ;
	echoReq.icmpHdr.Code		= 0;
	echoReq.icmpHdr.ID			= 123;
	echoReq.icmpHdr.Seq			= 456;
	for (unsigned i = 0; i < sizeof(echoReq.cData); i++)
		echoReq.cData[i] = ' ' + i;
	echoReq.dwTime				= GetTickCount();
	echoReq.icmpHdr.Checksum = in_cksum((u_short *)&echoReq, sizeof(Request));

    if (SOCKET_ERROR == sendto(rawSocket, (LPSTR)&echoReq, sizeof(Request), 0, (LPSOCKADDR)&saDest, sizeof(SOCKADDR_IN))) {
        VERBOSE(VB_SOCKET, "Ping: send failed");
        closesocket(rawSocket);
        return false;
    }

	struct timeval Timeout;
	fd_set readfds;
	readfds.fd_count = 1;
	readfds.fd_array[0] = rawSocket;
	Timeout.tv_sec = timeout;
    Timeout.tv_usec = 0;

    if (SOCKET_ERROR == select(1, &readfds, NULL, NULL, &Timeout)) {
        VERBOSE(VB_SOCKET, "Ping: timeout expired or select failed");
        closesocket(rawSocket);
        return false;
    }

    closesocket(rawSocket);
    VERBOSE(VB_SOCKET, "Ping: done");
    return true;
#else
    QString cmd = QString("ping -t %1 -c 1  %2  >/dev/null 2>&1")
                  .arg(timeout).arg(host);

    if (myth_system(cmd))
    {
        // ping command may not like -t argument. Simplify:

        cmd = QString("ping -c 1  %2  >/dev/null 2>&1").arg(host);

        if (myth_system(cmd))
            return false;
    }

    return true;
#endif
}

/**
 * \brief Can we talk to port on host?
 */
bool telnet(const QString &host, int port)
{
    MythSocket *s = new MythSocket();
    
    if (s->connect(host, port))
    {
        s->close();
        return true;
    }

    return false;
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
 *   is succesful the file pointers will be at the end of the copied
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
        rlen = src.readBlock(buf, buflen);
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
            wlen = dst.writeBlock(buf + off, rlen - off);
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
            ret = mkdir(tempfilename);
        else
            ret = open(tempfilename, O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
    }
    QString tmpFileName(tempfilename);
#else
    const char *tmp = name_template.ascii();
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

double MythGetPixelAspectRatio(void)
{
    float pixelAspect = 1.0;
#ifdef USING_X11
    pixelAspect = MythXGetPixelAspectRatio();
#endif // USING_X11
    return pixelAspect;
}

unsigned long long myth_get_approximate_large_file_size(const QString &fname)
{
    // .local8Bit() not thread-safe.. even with Qt4, make a deep copy first..
    QString filename = Q3DeepCopy<QString>(fname);
#ifdef USING_MINGW
    struct _stati64 status;
    _stati64(filename.local8Bit(), &status);
    return status.st_size;
#else
    struct stat status;
    if (stat(filename.local8Bit(), &status) == -1)
        return 0;

    // Using off_t requires a lot of 32/64 bit checking.
    // So just get the size in blocks.
    unsigned long long bsize = status.st_blksize;
    unsigned long long nblk  = status.st_blocks;
    unsigned long long approx_size = nblk * bsize;
    return approx_size;
#endif
}

QColor createColor(const QString &color)
{
    static QMutex x11colormapLock;
    static QMap<QString,QColor> x11colormap;

    QMutexLocker locker(&x11colormapLock);
    if (x11colormap.empty())
    {
        x11colormap["snow"] = QColor(255, 250, 250);
        x11colormap["ghost"] = QColor(248, 248, 255);
        x11colormap["ghostwhite"] = QColor(248, 248, 255);
        x11colormap["white"] = QColor(245, 245, 245);
        x11colormap["whitesmoke"] = QColor(245, 245, 245);
        x11colormap["gainsboro"] = QColor(220, 220, 220);
        x11colormap["floral"] = QColor(255, 250, 240);
        x11colormap["floralwhite"] = QColor(255, 250, 240);
        x11colormap["old"] = QColor(253, 245, 230);
        x11colormap["oldlace"] = QColor(253, 245, 230);
        x11colormap["linen"] = QColor(250, 240, 230);
        x11colormap["antique"] = QColor(250, 235, 215);
        x11colormap["antiquewhite"] = QColor(250, 235, 215);
        x11colormap["papaya"] = QColor(255, 239, 213);
        x11colormap["papayawhip"] = QColor(255, 239, 213);
        x11colormap["blanched"] = QColor(255, 235, 205);
        x11colormap["blanchedalmond"] = QColor(255, 235, 205);
        x11colormap["bisque"] = QColor(255, 228, 196);
        x11colormap["peach"] = QColor(255, 218, 185);
        x11colormap["peachpuff"] = QColor(255, 218, 185);
        x11colormap["navajo"] = QColor(255, 222, 173);
        x11colormap["navajowhite"] = QColor(255, 222, 173);
        x11colormap["moccasin"] = QColor(255, 228, 181);
        x11colormap["cornsilk"] = QColor(255, 248, 220);
        x11colormap["ivory"] = QColor(255, 255, 240);
        x11colormap["lemon"] = QColor(255, 250, 205);
        x11colormap["lemonchiffon"] = QColor(255, 250, 205);
        x11colormap["seashell"] = QColor(255, 245, 238);
        x11colormap["honeydew"] = QColor(240, 255, 240);
        x11colormap["mint"] = QColor(245, 255, 250);
        x11colormap["mintcream"] = QColor(245, 255, 250);
        x11colormap["azure"] = QColor(240, 255, 255);
        x11colormap["alice"] = QColor(240, 248, 255);
        x11colormap["aliceblue"] = QColor(240, 248, 255);
        x11colormap["lavender"] = QColor(230, 230, 250);
        x11colormap["lavender"] = QColor(255, 240, 245);
        x11colormap["lavenderblush"] = QColor(255, 240, 245);
        x11colormap["misty"] = QColor(255, 228, 225);
        x11colormap["mistyrose"] = QColor(255, 228, 225);
        x11colormap["white"] = QColor(255, 255, 255);
        x11colormap["black"] = QColor(0, 0, 0);
        x11colormap["dark"] = QColor(47, 79, 79);
        x11colormap["darkslategray"] = QColor(47, 79, 79);
        x11colormap["dark"] = QColor(47, 79, 79);
        x11colormap["darkslategrey"] = QColor(47, 79, 79);
        x11colormap["dim"] = QColor(105, 105, 105);
        x11colormap["dimgray"] = QColor(105, 105, 105);
        x11colormap["dim"] = QColor(105, 105, 105);
        x11colormap["dimgrey"] = QColor(105, 105, 105);
        x11colormap["slate"] = QColor(112, 128, 144);
        x11colormap["slategray"] = QColor(112, 128, 144);
        x11colormap["slate"] = QColor(112, 128, 144);
        x11colormap["slategrey"] = QColor(112, 128, 144);
        x11colormap["light"] = QColor(119, 136, 153);
        x11colormap["lightslategray"] = QColor(119, 136, 153);
        x11colormap["light"] = QColor(119, 136, 153);
        x11colormap["lightslategrey"] = QColor(119, 136, 153);
        x11colormap["gray"] = QColor(190, 190, 190);
        x11colormap["grey"] = QColor(190, 190, 190);
        x11colormap["light"] = QColor(211, 211, 211);
        x11colormap["lightgrey"] = QColor(211, 211, 211);
        x11colormap["light"] = QColor(211, 211, 211);
        x11colormap["lightgray"] = QColor(211, 211, 211);
        x11colormap["midnight"] = QColor(25, 25, 112);
        x11colormap["midnightblue"] = QColor(25, 25, 112);
        x11colormap["navy"] = QColor(0, 0, 128);
        x11colormap["navy"] = QColor(0, 0, 128);
        x11colormap["navyblue"] = QColor(0, 0, 128);
        x11colormap["cornflower"] = QColor(100, 149, 237);
        x11colormap["cornflowerblue"] = QColor(100, 149, 237);
        x11colormap["dark"] = QColor(72, 61, 139);
        x11colormap["darkslateblue"] = QColor(72, 61, 139);
        x11colormap["slate"] = QColor(106, 90, 205);
        x11colormap["slateblue"] = QColor(106, 90, 205);
        x11colormap["medium"] = QColor(123, 104, 238);
        x11colormap["mediumslateblue"] = QColor(123, 104, 238);
        x11colormap["light"] = QColor(132, 112, 255);
        x11colormap["lightslateblue"] = QColor(132, 112, 255);
        x11colormap["medium"] = QColor(0, 0, 205);
        x11colormap["mediumblue"] = QColor(0, 0, 205);
        x11colormap["royal"] = QColor(65, 105, 225);
        x11colormap["royalblue"] = QColor(65, 105, 225);
        x11colormap["blue"] = QColor(0, 0, 255);
        x11colormap["dodger"] = QColor(30, 144, 255);
        x11colormap["dodgerblue"] = QColor(30, 144, 255);
        x11colormap["deep"] = QColor(0, 191, 255);
        x11colormap["deepskyblue"] = QColor(0, 191, 255);
        x11colormap["sky"] = QColor(135, 206, 235);
        x11colormap["skyblue"] = QColor(135, 206, 235);
        x11colormap["light"] = QColor(135, 206, 250);
        x11colormap["lightskyblue"] = QColor(135, 206, 250);
        x11colormap["steel"] = QColor(70, 130, 180);
        x11colormap["steelblue"] = QColor(70, 130, 180);
        x11colormap["light"] = QColor(176, 196, 222);
        x11colormap["lightsteelblue"] = QColor(176, 196, 222);
        x11colormap["light"] = QColor(173, 216, 230);
        x11colormap["lightblue"] = QColor(173, 216, 230);
        x11colormap["powder"] = QColor(176, 224, 230);
        x11colormap["powderblue"] = QColor(176, 224, 230);
        x11colormap["pale"] = QColor(175, 238, 238);
        x11colormap["paleturquoise"] = QColor(175, 238, 238);
        x11colormap["dark"] = QColor(0, 206, 209);
        x11colormap["darkturquoise"] = QColor(0, 206, 209);
        x11colormap["medium"] = QColor(72, 209, 204);
        x11colormap["mediumturquoise"] = QColor(72, 209, 204);
        x11colormap["turquoise"] = QColor(64, 224, 208);
        x11colormap["cyan"] = QColor(0, 255, 255);
        x11colormap["light"] = QColor(224, 255, 255);
        x11colormap["lightcyan"] = QColor(224, 255, 255);
        x11colormap["cadet"] = QColor(95, 158, 160);
        x11colormap["cadetblue"] = QColor(95, 158, 160);
        x11colormap["medium"] = QColor(102, 205, 170);
        x11colormap["mediumaquamarine"] = QColor(102, 205, 170);
        x11colormap["aquamarine"] = QColor(127, 255, 212);
        x11colormap["dark"] = QColor(0, 100, 0);
        x11colormap["darkgreen"] = QColor(0, 100, 0);
        x11colormap["dark"] = QColor(85, 107, 47);
        x11colormap["darkolivegreen"] = QColor(85, 107, 47);
        x11colormap["dark"] = QColor(143, 188, 143);
        x11colormap["darkseagreen"] = QColor(143, 188, 143);
        x11colormap["sea"] = QColor(46, 139, 87);
        x11colormap["seagreen"] = QColor(46, 139, 87);
        x11colormap["medium"] = QColor(60, 179, 113);
        x11colormap["mediumseagreen"] = QColor(60, 179, 113);
        x11colormap["light"] = QColor(32, 178, 170);
        x11colormap["lightseagreen"] = QColor(32, 178, 170);
        x11colormap["pale"] = QColor(152, 251, 152);
        x11colormap["palegreen"] = QColor(152, 251, 152);
        x11colormap["spring"] = QColor(0, 255, 127);
        x11colormap["springgreen"] = QColor(0, 255, 127);
        x11colormap["lawn"] = QColor(124, 252, 0);
        x11colormap["lawngreen"] = QColor(124, 252, 0);
        x11colormap["green"] = QColor(0, 255, 0);
        x11colormap["chartreuse"] = QColor(127, 255, 0);
        x11colormap["medium"] = QColor(0, 250, 154);
        x11colormap["mediumspringgreen"] = QColor(0, 250, 154);
        x11colormap["green"] = QColor(173, 255, 47);
        x11colormap["greenyellow"] = QColor(173, 255, 47);
        x11colormap["lime"] = QColor(50, 205, 50);
        x11colormap["limegreen"] = QColor(50, 205, 50);
        x11colormap["yellow"] = QColor(154, 205, 50);
        x11colormap["yellowgreen"] = QColor(154, 205, 50);
        x11colormap["forest"] = QColor(34, 139, 34);
        x11colormap["forestgreen"] = QColor(34, 139, 34);
        x11colormap["olive"] = QColor(107, 142, 35);
        x11colormap["olivedrab"] = QColor(107, 142, 35);
        x11colormap["dark"] = QColor(189, 183, 107);
        x11colormap["darkkhaki"] = QColor(189, 183, 107);
        x11colormap["khaki"] = QColor(240, 230, 140);
        x11colormap["pale"] = QColor(238, 232, 170);
        x11colormap["palegoldenrod"] = QColor(238, 232, 170);
        x11colormap["light"] = QColor(250, 250, 210);
        x11colormap["lightgoldenrodyellow"] = QColor(250, 250, 210);
        x11colormap["light"] = QColor(255, 255, 224);
        x11colormap["lightyellow"] = QColor(255, 255, 224);
        x11colormap["yellow"] = QColor(255, 255, 0);
        x11colormap["gold"] = QColor(255, 215, 0);
        x11colormap["light"] = QColor(238, 221, 130);
        x11colormap["lightgoldenrod"] = QColor(238, 221, 130);
        x11colormap["goldenrod"] = QColor(218, 165, 32);
        x11colormap["dark"] = QColor(184, 134, 11);
        x11colormap["darkgoldenrod"] = QColor(184, 134, 11);
        x11colormap["rosy"] = QColor(188, 143, 143);
        x11colormap["rosybrown"] = QColor(188, 143, 143);
        x11colormap["indian"] = QColor(205, 92, 92);
        x11colormap["indianred"] = QColor(205, 92, 92);
        x11colormap["saddle"] = QColor(139, 69, 19);
        x11colormap["saddlebrown"] = QColor(139, 69, 19);
        x11colormap["sienna"] = QColor(160, 82, 45);
        x11colormap["peru"] = QColor(205, 133, 63);
        x11colormap["burlywood"] = QColor(222, 184, 135);
        x11colormap["beige"] = QColor(245, 245, 220);
        x11colormap["wheat"] = QColor(245, 222, 179);
        x11colormap["sandy"] = QColor(244, 164, 96);
        x11colormap["sandybrown"] = QColor(244, 164, 96);
        x11colormap["tan"] = QColor(210, 180, 140);
        x11colormap["chocolate"] = QColor(210, 105, 30);
        x11colormap["firebrick"] = QColor(178, 34, 34);
        x11colormap["brown"] = QColor(165, 42, 42);
        x11colormap["dark"] = QColor(233, 150, 122);
        x11colormap["darksalmon"] = QColor(233, 150, 122);
        x11colormap["salmon"] = QColor(250, 128, 114);
        x11colormap["light"] = QColor(255, 160, 122);
        x11colormap["lightsalmon"] = QColor(255, 160, 122);
        x11colormap["orange"] = QColor(255, 165, 0);
        x11colormap["dark"] = QColor(255, 140, 0);
        x11colormap["darkorange"] = QColor(255, 140, 0);
        x11colormap["coral"] = QColor(255, 127, 80);
        x11colormap["light"] = QColor(240, 128, 128);
        x11colormap["lightcoral"] = QColor(240, 128, 128);
        x11colormap["tomato"] = QColor(255, 99, 71);
        x11colormap["orange"] = QColor(255, 69, 0);
        x11colormap["orangered"] = QColor(255, 69, 0);
        x11colormap["red"] = QColor(255, 0, 0);
        x11colormap["hot"] = QColor(255, 105, 180);
        x11colormap["hotpink"] = QColor(255, 105, 180);
        x11colormap["deep"] = QColor(255, 20, 147);
        x11colormap["deeppink"] = QColor(255, 20, 147);
        x11colormap["pink"] = QColor(255, 192, 203);
        x11colormap["light"] = QColor(255, 182, 193);
        x11colormap["lightpink"] = QColor(255, 182, 193);
        x11colormap["pale"] = QColor(219, 112, 147);
        x11colormap["palevioletred"] = QColor(219, 112, 147);
        x11colormap["maroon"] = QColor(176, 48, 96);
        x11colormap["medium"] = QColor(199, 21, 133);
        x11colormap["mediumvioletred"] = QColor(199, 21, 133);
        x11colormap["violet"] = QColor(208, 32, 144);
        x11colormap["violetred"] = QColor(208, 32, 144);
        x11colormap["magenta"] = QColor(255, 0, 255);
        x11colormap["violet"] = QColor(238, 130, 238);
        x11colormap["plum"] = QColor(221, 160, 221);
        x11colormap["orchid"] = QColor(218, 112, 214);
        x11colormap["medium"] = QColor(186, 85, 211);
        x11colormap["mediumorchid"] = QColor(186, 85, 211);
        x11colormap["dark"] = QColor(153, 50, 204);
        x11colormap["darkorchid"] = QColor(153, 50, 204);
        x11colormap["dark"] = QColor(148, 0, 211);
        x11colormap["darkviolet"] = QColor(148, 0, 211);
        x11colormap["blue"] = QColor(138, 43, 226);
        x11colormap["blueviolet"] = QColor(138, 43, 226);
        x11colormap["purple"] = QColor(160, 32, 240);
        x11colormap["medium"] = QColor(147, 112, 219);
        x11colormap["mediumpurple"] = QColor(147, 112, 219);
        x11colormap["thistle"] = QColor(216, 191, 216);
        x11colormap["snow1"] = QColor(255, 250, 250);
        x11colormap["snow2"] = QColor(238, 233, 233);
        x11colormap["snow3"] = QColor(205, 201, 201);
        x11colormap["snow4"] = QColor(139, 137, 137);
        x11colormap["seashell1"] = QColor(255, 245, 238);
        x11colormap["seashell2"] = QColor(238, 229, 222);
        x11colormap["seashell3"] = QColor(205, 197, 191);
        x11colormap["seashell4"] = QColor(139, 134, 130);
        x11colormap["antiquewhite1"] = QColor(255, 239, 219);
        x11colormap["antiquewhite2"] = QColor(238, 223, 204);
        x11colormap["antiquewhite3"] = QColor(205, 192, 176);
        x11colormap["antiquewhite4"] = QColor(139, 131, 120);
        x11colormap["bisque1"] = QColor(255, 228, 196);
        x11colormap["bisque2"] = QColor(238, 213, 183);
        x11colormap["bisque3"] = QColor(205, 183, 158);
        x11colormap["bisque4"] = QColor(139, 125, 107);
        x11colormap["peachpuff1"] = QColor(255, 218, 185);
        x11colormap["peachpuff2"] = QColor(238, 203, 173);
        x11colormap["peachpuff3"] = QColor(205, 175, 149);
        x11colormap["peachpuff4"] = QColor(139, 119, 101);
        x11colormap["navajowhite1"] = QColor(255, 222, 173);
        x11colormap["navajowhite2"] = QColor(238, 207, 161);
        x11colormap["navajowhite3"] = QColor(205, 179, 139);
        x11colormap["navajowhite4"] = QColor(139, 121, 94);
        x11colormap["lemonchiffon1"] = QColor(255, 250, 205);
        x11colormap["lemonchiffon2"] = QColor(238, 233, 191);
        x11colormap["lemonchiffon3"] = QColor(205, 201, 165);
        x11colormap["lemonchiffon4"] = QColor(139, 137, 112);
        x11colormap["cornsilk1"] = QColor(255, 248, 220);
        x11colormap["cornsilk2"] = QColor(238, 232, 205);
        x11colormap["cornsilk3"] = QColor(205, 200, 177);
        x11colormap["cornsilk4"] = QColor(139, 136, 120);
        x11colormap["ivory1"] = QColor(255, 255, 240);
        x11colormap["ivory2"] = QColor(238, 238, 224);
        x11colormap["ivory3"] = QColor(205, 205, 193);
        x11colormap["ivory4"] = QColor(139, 139, 131);
        x11colormap["honeydew1"] = QColor(240, 255, 240);
        x11colormap["honeydew2"] = QColor(224, 238, 224);
        x11colormap["honeydew3"] = QColor(193, 205, 193);
        x11colormap["honeydew4"] = QColor(131, 139, 131);
        x11colormap["lavenderblush1"] = QColor(255, 240, 245);
        x11colormap["lavenderblush2"] = QColor(238, 224, 229);
        x11colormap["lavenderblush3"] = QColor(205, 193, 197);
        x11colormap["lavenderblush4"] = QColor(139, 131, 134);
        x11colormap["mistyrose1"] = QColor(255, 228, 225);
        x11colormap["mistyrose2"] = QColor(238, 213, 210);
        x11colormap["mistyrose3"] = QColor(205, 183, 181);
        x11colormap["mistyrose4"] = QColor(139, 125, 123);
        x11colormap["azure1"] = QColor(240, 255, 255);
        x11colormap["azure2"] = QColor(224, 238, 238);
        x11colormap["azure3"] = QColor(193, 205, 205);
        x11colormap["azure4"] = QColor(131, 139, 139);
        x11colormap["slateblue1"] = QColor(131, 111, 255);
        x11colormap["slateblue2"] = QColor(122, 103, 238);
        x11colormap["slateblue3"] = QColor(105, 89, 205);
        x11colormap["slateblue4"] = QColor(71, 60, 139);
        x11colormap["royalblue1"] = QColor(72, 118, 255);
        x11colormap["royalblue2"] = QColor(67, 110, 238);
        x11colormap["royalblue3"] = QColor(58, 95, 205);
        x11colormap["royalblue4"] = QColor(39, 64, 139);
        x11colormap["blue1"] = QColor(0, 0, 255);
        x11colormap["blue2"] = QColor(0, 0, 238);
        x11colormap["blue3"] = QColor(0, 0, 205);
        x11colormap["blue4"] = QColor(0, 0, 139);
        x11colormap["dodgerblue1"] = QColor(30, 144, 255);
        x11colormap["dodgerblue2"] = QColor(28, 134, 238);
        x11colormap["dodgerblue3"] = QColor(24, 116, 205);
        x11colormap["dodgerblue4"] = QColor(16, 78, 139);
        x11colormap["steelblue1"] = QColor(99, 184, 255);
        x11colormap["steelblue2"] = QColor(92, 172, 238);
        x11colormap["steelblue3"] = QColor(79, 148, 205);
        x11colormap["steelblue4"] = QColor(54, 100, 139);
        x11colormap["deepskyblue1"] = QColor(0, 191, 255);
        x11colormap["deepskyblue2"] = QColor(0, 178, 238);
        x11colormap["deepskyblue3"] = QColor(0, 154, 205);
        x11colormap["deepskyblue4"] = QColor(0, 104, 139);
        x11colormap["skyblue1"] = QColor(135, 206, 255);
        x11colormap["skyblue2"] = QColor(126, 192, 238);
        x11colormap["skyblue3"] = QColor(108, 166, 205);
        x11colormap["skyblue4"] = QColor(74, 112, 139);
        x11colormap["lightskyblue1"] = QColor(176, 226, 255);
        x11colormap["lightskyblue2"] = QColor(164, 211, 238);
        x11colormap["lightskyblue3"] = QColor(141, 182, 205);
        x11colormap["lightskyblue4"] = QColor(96, 123, 139);
        x11colormap["slategray1"] = QColor(198, 226, 255);
        x11colormap["slategray2"] = QColor(185, 211, 238);
        x11colormap["slategray3"] = QColor(159, 182, 205);
        x11colormap["slategray4"] = QColor(108, 123, 139);
        x11colormap["lightsteelblue1"] = QColor(202, 225, 255);
        x11colormap["lightsteelblue2"] = QColor(188, 210, 238);
        x11colormap["lightsteelblue3"] = QColor(162, 181, 205);
        x11colormap["lightsteelblue4"] = QColor(110, 123, 139);
        x11colormap["lightblue1"] = QColor(191, 239, 255);
        x11colormap["lightblue2"] = QColor(178, 223, 238);
        x11colormap["lightblue3"] = QColor(154, 192, 205);
        x11colormap["lightblue4"] = QColor(104, 131, 139);
        x11colormap["lightcyan1"] = QColor(224, 255, 255);
        x11colormap["lightcyan2"] = QColor(209, 238, 238);
        x11colormap["lightcyan3"] = QColor(180, 205, 205);
        x11colormap["lightcyan4"] = QColor(122, 139, 139);
        x11colormap["paleturquoise1"] = QColor(187, 255, 255);
        x11colormap["paleturquoise2"] = QColor(174, 238, 238);
        x11colormap["paleturquoise3"] = QColor(150, 205, 205);
        x11colormap["paleturquoise4"] = QColor(102, 139, 139);
        x11colormap["cadetblue1"] = QColor(152, 245, 255);
        x11colormap["cadetblue2"] = QColor(142, 229, 238);
        x11colormap["cadetblue3"] = QColor(122, 197, 205);
        x11colormap["cadetblue4"] = QColor(83, 134, 139);
        x11colormap["turquoise1"] = QColor(0, 245, 255);
        x11colormap["turquoise2"] = QColor(0, 229, 238);
        x11colormap["turquoise3"] = QColor(0, 197, 205);
        x11colormap["turquoise4"] = QColor(0, 134, 139);
        x11colormap["cyan1"] = QColor(0, 255, 255);
        x11colormap["cyan2"] = QColor(0, 238, 238);
        x11colormap["cyan3"] = QColor(0, 205, 205);
        x11colormap["cyan4"] = QColor(0, 139, 139);
        x11colormap["darkslategray1"] = QColor(151, 255, 255);
        x11colormap["darkslategray2"] = QColor(141, 238, 238);
        x11colormap["darkslategray3"] = QColor(121, 205, 205);
        x11colormap["darkslategray4"] = QColor(82, 139, 139);
        x11colormap["aquamarine1"] = QColor(127, 255, 212);
        x11colormap["aquamarine2"] = QColor(118, 238, 198);
        x11colormap["aquamarine3"] = QColor(102, 205, 170);
        x11colormap["aquamarine4"] = QColor(69, 139, 116);
        x11colormap["darkseagreen1"] = QColor(193, 255, 193);
        x11colormap["darkseagreen2"] = QColor(180, 238, 180);
        x11colormap["darkseagreen3"] = QColor(155, 205, 155);
        x11colormap["darkseagreen4"] = QColor(105, 139, 105);
        x11colormap["seagreen1"] = QColor(84, 255, 159);
        x11colormap["seagreen2"] = QColor(78, 238, 148);
        x11colormap["seagreen3"] = QColor(67, 205, 128);
        x11colormap["seagreen4"] = QColor(46, 139, 87);
        x11colormap["palegreen1"] = QColor(154, 255, 154);
        x11colormap["palegreen2"] = QColor(144, 238, 144);
        x11colormap["palegreen3"] = QColor(124, 205, 124);
        x11colormap["palegreen4"] = QColor(84, 139, 84);
        x11colormap["springgreen1"] = QColor(0, 255, 127);
        x11colormap["springgreen2"] = QColor(0, 238, 118);
        x11colormap["springgreen3"] = QColor(0, 205, 102);
        x11colormap["springgreen4"] = QColor(0, 139, 69);
        x11colormap["green1"] = QColor(0, 255, 0);
        x11colormap["green2"] = QColor(0, 238, 0);
        x11colormap["green3"] = QColor(0, 205, 0);
        x11colormap["green4"] = QColor(0, 139, 0);
        x11colormap["chartreuse1"] = QColor(127, 255, 0);
        x11colormap["chartreuse2"] = QColor(118, 238, 0);
        x11colormap["chartreuse3"] = QColor(102, 205, 0);
        x11colormap["chartreuse4"] = QColor(69, 139, 0);
        x11colormap["olivedrab1"] = QColor(192, 255, 62);
        x11colormap["olivedrab2"] = QColor(179, 238, 58);
        x11colormap["olivedrab3"] = QColor(154, 205, 50);
        x11colormap["olivedrab4"] = QColor(105, 139, 34);
        x11colormap["darkolivegreen1"] = QColor(202, 255, 112);
        x11colormap["darkolivegreen2"] = QColor(188, 238, 104);
        x11colormap["darkolivegreen3"] = QColor(162, 205, 90);
        x11colormap["darkolivegreen4"] = QColor(110, 139, 61);
        x11colormap["khaki1"] = QColor(255, 246, 143);
        x11colormap["khaki2"] = QColor(238, 230, 133);
        x11colormap["khaki3"] = QColor(205, 198, 115);
        x11colormap["khaki4"] = QColor(139, 134, 78);
        x11colormap["lightgoldenrod1"] = QColor(255, 236, 139);
        x11colormap["lightgoldenrod2"] = QColor(238, 220, 130);
        x11colormap["lightgoldenrod3"] = QColor(205, 190, 112);
        x11colormap["lightgoldenrod4"] = QColor(139, 129, 76);
        x11colormap["lightyellow1"] = QColor(255, 255, 224);
        x11colormap["lightyellow2"] = QColor(238, 238, 209);
        x11colormap["lightyellow3"] = QColor(205, 205, 180);
        x11colormap["lightyellow4"] = QColor(139, 139, 122);
        x11colormap["yellow1"] = QColor(255, 255, 0);
        x11colormap["yellow2"] = QColor(238, 238, 0);
        x11colormap["yellow3"] = QColor(205, 205, 0);
        x11colormap["yellow4"] = QColor(139, 139, 0);
        x11colormap["gold1"] = QColor(255, 215, 0);
        x11colormap["gold2"] = QColor(238, 201, 0);
        x11colormap["gold3"] = QColor(205, 173, 0);
        x11colormap["gold4"] = QColor(139, 117, 0);
        x11colormap["goldenrod1"] = QColor(255, 193, 37);
        x11colormap["goldenrod2"] = QColor(238, 180, 34);
        x11colormap["goldenrod3"] = QColor(205, 155, 29);
        x11colormap["goldenrod4"] = QColor(139, 105, 20);
        x11colormap["darkgoldenrod1"] = QColor(255, 185, 15);
        x11colormap["darkgoldenrod2"] = QColor(238, 173, 14);
        x11colormap["darkgoldenrod3"] = QColor(205, 149, 12);
        x11colormap["darkgoldenrod4"] = QColor(139, 101, 8);
        x11colormap["rosybrown1"] = QColor(255, 193, 193);
        x11colormap["rosybrown2"] = QColor(238, 180, 180);
        x11colormap["rosybrown3"] = QColor(205, 155, 155);
        x11colormap["rosybrown4"] = QColor(139, 105, 105);
        x11colormap["indianred1"] = QColor(255, 106, 106);
        x11colormap["indianred2"] = QColor(238, 99, 99);
        x11colormap["indianred3"] = QColor(205, 85, 85);
        x11colormap["indianred4"] = QColor(139, 58, 58);
        x11colormap["sienna1"] = QColor(255, 130, 71);
        x11colormap["sienna2"] = QColor(238, 121, 66);
        x11colormap["sienna3"] = QColor(205, 104, 57);
        x11colormap["sienna4"] = QColor(139, 71, 38);
        x11colormap["burlywood1"] = QColor(255, 211, 155);
        x11colormap["burlywood2"] = QColor(238, 197, 145);
        x11colormap["burlywood3"] = QColor(205, 170, 125);
        x11colormap["burlywood4"] = QColor(139, 115, 85);
        x11colormap["wheat1"] = QColor(255, 231, 186);
        x11colormap["wheat2"] = QColor(238, 216, 174);
        x11colormap["wheat3"] = QColor(205, 186, 150);
        x11colormap["wheat4"] = QColor(139, 126, 102);
        x11colormap["tan1"] = QColor(255, 165, 79);
        x11colormap["tan2"] = QColor(238, 154, 73);
        x11colormap["tan3"] = QColor(205, 133, 63);
        x11colormap["tan4"] = QColor(139, 90, 43);
        x11colormap["chocolate1"] = QColor(255, 127, 36);
        x11colormap["chocolate2"] = QColor(238, 118, 33);
        x11colormap["chocolate3"] = QColor(205, 102, 29);
        x11colormap["chocolate4"] = QColor(139, 69, 19);
        x11colormap["firebrick1"] = QColor(255, 48, 48);
        x11colormap["firebrick2"] = QColor(238, 44, 44);
        x11colormap["firebrick3"] = QColor(205, 38, 38);
        x11colormap["firebrick4"] = QColor(139, 26, 26);
        x11colormap["brown1"] = QColor(255, 64, 64);
        x11colormap["brown2"] = QColor(238, 59, 59);
        x11colormap["brown3"] = QColor(205, 51, 51);
        x11colormap["brown4"] = QColor(139, 35, 35);
        x11colormap["salmon1"] = QColor(255, 140, 105);
        x11colormap["salmon2"] = QColor(238, 130, 98);
        x11colormap["salmon3"] = QColor(205, 112, 84);
        x11colormap["salmon4"] = QColor(139, 76, 57);
        x11colormap["lightsalmon1"] = QColor(255, 160, 122);
        x11colormap["lightsalmon2"] = QColor(238, 149, 114);
        x11colormap["lightsalmon3"] = QColor(205, 129, 98);
        x11colormap["lightsalmon4"] = QColor(139, 87, 66);
        x11colormap["orange1"] = QColor(255, 165, 0);
        x11colormap["orange2"] = QColor(238, 154, 0);
        x11colormap["orange3"] = QColor(205, 133, 0);
        x11colormap["orange4"] = QColor(139, 90, 0);
        x11colormap["darkorange1"] = QColor(255, 127, 0);
        x11colormap["darkorange2"] = QColor(238, 118, 0);
        x11colormap["darkorange3"] = QColor(205, 102, 0);
        x11colormap["darkorange4"] = QColor(139, 69, 0);
        x11colormap["coral1"] = QColor(255, 114, 86);
        x11colormap["coral2"] = QColor(238, 106, 80);
        x11colormap["coral3"] = QColor(205, 91, 69);
        x11colormap["coral4"] = QColor(139, 62, 47);
        x11colormap["tomato1"] = QColor(255, 99, 71);
        x11colormap["tomato2"] = QColor(238, 92, 66);
        x11colormap["tomato3"] = QColor(205, 79, 57);
        x11colormap["tomato4"] = QColor(139, 54, 38);
        x11colormap["orangered1"] = QColor(255, 69, 0);
        x11colormap["orangered2"] = QColor(238, 64, 0);
        x11colormap["orangered3"] = QColor(205, 55, 0);
        x11colormap["orangered4"] = QColor(139, 37, 0);
        x11colormap["red1"] = QColor(255, 0, 0);
        x11colormap["red2"] = QColor(238, 0, 0);
        x11colormap["red3"] = QColor(205, 0, 0);
        x11colormap["red4"] = QColor(139, 0, 0);
        x11colormap["deeppink1"] = QColor(255, 20, 147);
        x11colormap["deeppink2"] = QColor(238, 18, 137);
        x11colormap["deeppink3"] = QColor(205, 16, 118);
        x11colormap["deeppink4"] = QColor(139, 10, 80);
        x11colormap["hotpink1"] = QColor(255, 110, 180);
        x11colormap["hotpink2"] = QColor(238, 106, 167);
        x11colormap["hotpink3"] = QColor(205, 96, 144);
        x11colormap["hotpink4"] = QColor(139, 58, 98);
        x11colormap["pink1"] = QColor(255, 181, 197);
        x11colormap["pink2"] = QColor(238, 169, 184);
        x11colormap["pink3"] = QColor(205, 145, 158);
        x11colormap["pink4"] = QColor(139, 99, 108);
        x11colormap["lightpink1"] = QColor(255, 174, 185);
        x11colormap["lightpink2"] = QColor(238, 162, 173);
        x11colormap["lightpink3"] = QColor(205, 140, 149);
        x11colormap["lightpink4"] = QColor(139, 95, 101);
        x11colormap["palevioletred1"] = QColor(255, 130, 171);
        x11colormap["palevioletred2"] = QColor(238, 121, 159);
        x11colormap["palevioletred3"] = QColor(205, 104, 137);
        x11colormap["palevioletred4"] = QColor(139, 71, 93);
        x11colormap["maroon1"] = QColor(255, 52, 179);
        x11colormap["maroon2"] = QColor(238, 48, 167);
        x11colormap["maroon3"] = QColor(205, 41, 144);
        x11colormap["maroon4"] = QColor(139, 28, 98);
        x11colormap["violetred1"] = QColor(255, 62, 150);
        x11colormap["violetred2"] = QColor(238, 58, 140);
        x11colormap["violetred3"] = QColor(205, 50, 120);
        x11colormap["violetred4"] = QColor(139, 34, 82);
        x11colormap["magenta1"] = QColor(255, 0, 255);
        x11colormap["magenta2"] = QColor(238, 0, 238);
        x11colormap["magenta3"] = QColor(205, 0, 205);
        x11colormap["magenta4"] = QColor(139, 0, 139);
        x11colormap["orchid1"] = QColor(255, 131, 250);
        x11colormap["orchid2"] = QColor(238, 122, 233);
        x11colormap["orchid3"] = QColor(205, 105, 201);
        x11colormap["orchid4"] = QColor(139, 71, 137);
        x11colormap["plum1"] = QColor(255, 187, 255);
        x11colormap["plum2"] = QColor(238, 174, 238);
        x11colormap["plum3"] = QColor(205, 150, 205);
        x11colormap["plum4"] = QColor(139, 102, 139);
        x11colormap["mediumorchid1"] = QColor(224, 102, 255);
        x11colormap["mediumorchid2"] = QColor(209, 95, 238);
        x11colormap["mediumorchid3"] = QColor(180, 82, 205);
        x11colormap["mediumorchid4"] = QColor(122, 55, 139);
        x11colormap["darkorchid1"] = QColor(191, 62, 255);
        x11colormap["darkorchid2"] = QColor(178, 58, 238);
        x11colormap["darkorchid3"] = QColor(154, 50, 205);
        x11colormap["darkorchid4"] = QColor(104, 34, 139);
        x11colormap["purple1"] = QColor(155, 48, 255);
        x11colormap["purple2"] = QColor(145, 44, 238);
        x11colormap["purple3"] = QColor(125, 38, 205);
        x11colormap["purple4"] = QColor(85, 26, 139);
        x11colormap["mediumpurple1"] = QColor(171, 130, 255);
        x11colormap["mediumpurple2"] = QColor(159, 121, 238);
        x11colormap["mediumpurple3"] = QColor(137, 104, 205);
        x11colormap["mediumpurple4"] = QColor(93, 71, 139);
        x11colormap["thistle1"] = QColor(255, 225, 255);
        x11colormap["thistle2"] = QColor(238, 210, 238);
        x11colormap["thistle3"] = QColor(205, 181, 205);
        x11colormap["thistle4"] = QColor(139, 123, 139);
        x11colormap["gray0"] = QColor(0, 0, 0);
        x11colormap["grey0"] = QColor(0, 0, 0);
        x11colormap["gray1"] = QColor(3, 3, 3);
        x11colormap["grey1"] = QColor(3, 3, 3);
        x11colormap["gray2"] = QColor(5, 5, 5);
        x11colormap["grey2"] = QColor(5, 5, 5);
        x11colormap["gray3"] = QColor(8, 8, 8);
        x11colormap["grey3"] = QColor(8, 8, 8);
        x11colormap["gray4"] = QColor(10, 10, 10);
        x11colormap["grey4"] = QColor(10, 10, 10);
        x11colormap["gray5"] = QColor(13, 13, 13);
        x11colormap["grey5"] = QColor(13, 13, 13);
        x11colormap["gray6"] = QColor(15, 15, 15);
        x11colormap["grey6"] = QColor(15, 15, 15);
        x11colormap["gray7"] = QColor(18, 18, 18);
        x11colormap["grey7"] = QColor(18, 18, 18);
        x11colormap["gray8"] = QColor(20, 20, 20);
        x11colormap["grey8"] = QColor(20, 20, 20);
        x11colormap["gray9"] = QColor(23, 23, 23);
        x11colormap["grey9"] = QColor(23, 23, 23);
        x11colormap["gray10"] = QColor(26, 26, 26);
        x11colormap["grey10"] = QColor(26, 26, 26);
        x11colormap["gray11"] = QColor(28, 28, 28);
        x11colormap["grey11"] = QColor(28, 28, 28);
        x11colormap["gray12"] = QColor(31, 31, 31);
        x11colormap["grey12"] = QColor(31, 31, 31);
        x11colormap["gray13"] = QColor(33, 33, 33);
        x11colormap["grey13"] = QColor(33, 33, 33);
        x11colormap["gray14"] = QColor(36, 36, 36);
        x11colormap["grey14"] = QColor(36, 36, 36);
        x11colormap["gray15"] = QColor(38, 38, 38);
        x11colormap["grey15"] = QColor(38, 38, 38);
        x11colormap["gray16"] = QColor(41, 41, 41);
        x11colormap["grey16"] = QColor(41, 41, 41);
        x11colormap["gray17"] = QColor(43, 43, 43);
        x11colormap["grey17"] = QColor(43, 43, 43);
        x11colormap["gray18"] = QColor(46, 46, 46);
        x11colormap["grey18"] = QColor(46, 46, 46);
        x11colormap["gray19"] = QColor(48, 48, 48);
        x11colormap["grey19"] = QColor(48, 48, 48);
        x11colormap["gray20"] = QColor(51, 51, 51);
        x11colormap["grey20"] = QColor(51, 51, 51);
        x11colormap["gray21"] = QColor(54, 54, 54);
        x11colormap["grey21"] = QColor(54, 54, 54);
        x11colormap["gray22"] = QColor(56, 56, 56);
        x11colormap["grey22"] = QColor(56, 56, 56);
        x11colormap["gray23"] = QColor(59, 59, 59);
        x11colormap["grey23"] = QColor(59, 59, 59);
        x11colormap["gray24"] = QColor(61, 61, 61);
        x11colormap["grey24"] = QColor(61, 61, 61);
        x11colormap["gray25"] = QColor(64, 64, 64);
        x11colormap["grey25"] = QColor(64, 64, 64);
        x11colormap["gray26"] = QColor(66, 66, 66);
        x11colormap["grey26"] = QColor(66, 66, 66);
        x11colormap["gray27"] = QColor(69, 69, 69);
        x11colormap["grey27"] = QColor(69, 69, 69);
        x11colormap["gray28"] = QColor(71, 71, 71);
        x11colormap["grey28"] = QColor(71, 71, 71);
        x11colormap["gray29"] = QColor(74, 74, 74);
        x11colormap["grey29"] = QColor(74, 74, 74);
        x11colormap["gray30"] = QColor(77, 77, 77);
        x11colormap["grey30"] = QColor(77, 77, 77);
        x11colormap["gray31"] = QColor(79, 79, 79);
        x11colormap["grey31"] = QColor(79, 79, 79);
        x11colormap["gray32"] = QColor(82, 82, 82);
        x11colormap["grey32"] = QColor(82, 82, 82);
        x11colormap["gray33"] = QColor(84, 84, 84);
        x11colormap["grey33"] = QColor(84, 84, 84);
        x11colormap["gray34"] = QColor(87, 87, 87);
        x11colormap["grey34"] = QColor(87, 87, 87);
        x11colormap["gray35"] = QColor(89, 89, 89);
        x11colormap["grey35"] = QColor(89, 89, 89);
        x11colormap["gray36"] = QColor(92, 92, 92);
        x11colormap["grey36"] = QColor(92, 92, 92);
        x11colormap["gray37"] = QColor(94, 94, 94);
        x11colormap["grey37"] = QColor(94, 94, 94);
        x11colormap["gray38"] = QColor(97, 97, 97);
        x11colormap["grey38"] = QColor(97, 97, 97);
        x11colormap["gray39"] = QColor(99, 99, 99);
        x11colormap["grey39"] = QColor(99, 99, 99);
        x11colormap["gray40"] = QColor(102, 102, 102);
        x11colormap["grey40"] = QColor(102, 102, 102);
        x11colormap["gray41"] = QColor(105, 105, 105);
        x11colormap["grey41"] = QColor(105, 105, 105);
        x11colormap["gray42"] = QColor(107, 107, 107);
        x11colormap["grey42"] = QColor(107, 107, 107);
        x11colormap["gray43"] = QColor(110, 110, 110);
        x11colormap["grey43"] = QColor(110, 110, 110);
        x11colormap["gray44"] = QColor(112, 112, 112);
        x11colormap["grey44"] = QColor(112, 112, 112);
        x11colormap["gray45"] = QColor(115, 115, 115);
        x11colormap["grey45"] = QColor(115, 115, 115);
        x11colormap["gray46"] = QColor(117, 117, 117);
        x11colormap["grey46"] = QColor(117, 117, 117);
        x11colormap["gray47"] = QColor(120, 120, 120);
        x11colormap["grey47"] = QColor(120, 120, 120);
        x11colormap["gray48"] = QColor(122, 122, 122);
        x11colormap["grey48"] = QColor(122, 122, 122);
        x11colormap["gray49"] = QColor(125, 125, 125);
        x11colormap["grey49"] = QColor(125, 125, 125);
        x11colormap["gray50"] = QColor(127, 127, 127);
        x11colormap["grey50"] = QColor(127, 127, 127);
        x11colormap["gray51"] = QColor(130, 130, 130);
        x11colormap["grey51"] = QColor(130, 130, 130);
        x11colormap["gray52"] = QColor(133, 133, 133);
        x11colormap["grey52"] = QColor(133, 133, 133);
        x11colormap["gray53"] = QColor(135, 135, 135);
        x11colormap["grey53"] = QColor(135, 135, 135);
        x11colormap["gray54"] = QColor(138, 138, 138);
        x11colormap["grey54"] = QColor(138, 138, 138);
        x11colormap["gray55"] = QColor(140, 140, 140);
        x11colormap["grey55"] = QColor(140, 140, 140);
        x11colormap["gray56"] = QColor(143, 143, 143);
        x11colormap["grey56"] = QColor(143, 143, 143);
        x11colormap["gray57"] = QColor(145, 145, 145);
        x11colormap["grey57"] = QColor(145, 145, 145);
        x11colormap["gray58"] = QColor(148, 148, 148);
        x11colormap["grey58"] = QColor(148, 148, 148);
        x11colormap["gray59"] = QColor(150, 150, 150);
        x11colormap["grey59"] = QColor(150, 150, 150);
        x11colormap["gray60"] = QColor(153, 153, 153);
        x11colormap["grey60"] = QColor(153, 153, 153);
        x11colormap["gray61"] = QColor(156, 156, 156);
        x11colormap["grey61"] = QColor(156, 156, 156);
        x11colormap["gray62"] = QColor(158, 158, 158);
        x11colormap["grey62"] = QColor(158, 158, 158);
        x11colormap["gray63"] = QColor(161, 161, 161);
        x11colormap["grey63"] = QColor(161, 161, 161);
        x11colormap["gray64"] = QColor(163, 163, 163);
        x11colormap["grey64"] = QColor(163, 163, 163);
        x11colormap["gray65"] = QColor(166, 166, 166);
        x11colormap["grey65"] = QColor(166, 166, 166);
        x11colormap["gray66"] = QColor(168, 168, 168);
        x11colormap["grey66"] = QColor(168, 168, 168);
        x11colormap["gray67"] = QColor(171, 171, 171);
        x11colormap["grey67"] = QColor(171, 171, 171);
        x11colormap["gray68"] = QColor(173, 173, 173);
        x11colormap["grey68"] = QColor(173, 173, 173);
        x11colormap["gray69"] = QColor(176, 176, 176);
        x11colormap["grey69"] = QColor(176, 176, 176);
        x11colormap["gray70"] = QColor(179, 179, 179);
        x11colormap["grey70"] = QColor(179, 179, 179);
        x11colormap["gray71"] = QColor(181, 181, 181);
        x11colormap["grey71"] = QColor(181, 181, 181);
        x11colormap["gray72"] = QColor(184, 184, 184);
        x11colormap["grey72"] = QColor(184, 184, 184);
        x11colormap["gray73"] = QColor(186, 186, 186);
        x11colormap["grey73"] = QColor(186, 186, 186);
        x11colormap["gray74"] = QColor(189, 189, 189);
        x11colormap["grey74"] = QColor(189, 189, 189);
        x11colormap["gray75"] = QColor(191, 191, 191);
        x11colormap["grey75"] = QColor(191, 191, 191);
        x11colormap["gray76"] = QColor(194, 194, 194);
        x11colormap["grey76"] = QColor(194, 194, 194);
        x11colormap["gray77"] = QColor(196, 196, 196);
        x11colormap["grey77"] = QColor(196, 196, 196);
        x11colormap["gray78"] = QColor(199, 199, 199);
        x11colormap["grey78"] = QColor(199, 199, 199);
        x11colormap["gray79"] = QColor(201, 201, 201);
        x11colormap["grey79"] = QColor(201, 201, 201);
        x11colormap["gray80"] = QColor(204, 204, 204);
        x11colormap["grey80"] = QColor(204, 204, 204);
        x11colormap["gray81"] = QColor(207, 207, 207);
        x11colormap["grey81"] = QColor(207, 207, 207);
        x11colormap["gray82"] = QColor(209, 209, 209);
        x11colormap["grey82"] = QColor(209, 209, 209);
        x11colormap["gray83"] = QColor(212, 212, 212);
        x11colormap["grey83"] = QColor(212, 212, 212);
        x11colormap["gray84"] = QColor(214, 214, 214);
        x11colormap["grey84"] = QColor(214, 214, 214);
        x11colormap["gray85"] = QColor(217, 217, 217);
        x11colormap["grey85"] = QColor(217, 217, 217);
        x11colormap["gray86"] = QColor(219, 219, 219);
        x11colormap["grey86"] = QColor(219, 219, 219);
        x11colormap["gray87"] = QColor(222, 222, 222);
        x11colormap["grey87"] = QColor(222, 222, 222);
        x11colormap["gray88"] = QColor(224, 224, 224);
        x11colormap["grey88"] = QColor(224, 224, 224);
        x11colormap["gray89"] = QColor(227, 227, 227);
        x11colormap["grey89"] = QColor(227, 227, 227);
        x11colormap["gray90"] = QColor(229, 229, 229);
        x11colormap["grey90"] = QColor(229, 229, 229);
        x11colormap["gray91"] = QColor(232, 232, 232);
        x11colormap["grey91"] = QColor(232, 232, 232);
        x11colormap["gray92"] = QColor(235, 235, 235);
        x11colormap["grey92"] = QColor(235, 235, 235);
        x11colormap["gray93"] = QColor(237, 237, 237);
        x11colormap["grey93"] = QColor(237, 237, 237);
        x11colormap["gray94"] = QColor(240, 240, 240);
        x11colormap["grey94"] = QColor(240, 240, 240);
        x11colormap["gray95"] = QColor(242, 242, 242);
        x11colormap["grey95"] = QColor(242, 242, 242);
        x11colormap["gray96"] = QColor(245, 245, 245);
        x11colormap["grey96"] = QColor(245, 245, 245);
        x11colormap["gray97"] = QColor(247, 247, 247);
        x11colormap["grey97"] = QColor(247, 247, 247);
        x11colormap["gray98"] = QColor(250, 250, 250);
        x11colormap["grey98"] = QColor(250, 250, 250);
        x11colormap["gray99"] = QColor(252, 252, 252);
        x11colormap["grey99"] = QColor(252, 252, 252);
        x11colormap["gray100"] = QColor(255, 255, 255);
        x11colormap["grey100"] = QColor(255, 255, 255);
        x11colormap["dark"] = QColor(169, 169, 169);
        x11colormap["darkgrey"] = QColor(169, 169, 169);
        x11colormap["dark"] = QColor(169, 169, 169);
        x11colormap["darkgray"] = QColor(169, 169, 169);
        x11colormap["dark"] = QColor(0, 0, 139);
        x11colormap["darkblue"] = QColor(0, 0, 139);
        x11colormap["dark"] = QColor(0, 139, 139);
        x11colormap["darkcyan"] = QColor(0, 139, 139);
        x11colormap["dark"] = QColor(139, 0, 139);
        x11colormap["darkmagenta"] = QColor(139, 0, 139);
        x11colormap["dark"] = QColor(139, 0, 0);
        x11colormap["darkred"] = QColor(139, 0, 0);
        x11colormap["light"] = QColor(144, 238, 144);
        x11colormap["lightgreen"] = QColor(144, 238, 144);
    }

    QMap<QString,QColor>::const_iterator it = x11colormap.find(color.lower());
    if (it != x11colormap.end())
        return *it;

    return QColor(color);
}
