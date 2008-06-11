// C++ headers
#include <iostream>
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
#include <qdeepcopy.h>

// Myth headers
#include "mythconfig.h"
#include "exitcodes.h"
#include "util.h"
#include "util-x11.h"
#include "mythcontext.h"
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
    if (offset >= list.size())
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
        execl("/bin/sh", "sh", "-c", QString(command.utf8()).ascii(), NULL);
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
    QString cmd = QString("cmd.exe /c %1").arg(command.utf8()).ascii();
    char* ch = new char[cmd.length() + 1];
	strcpy(ch, cmd);
    if (!::CreateProcessA(NULL, ch, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        delete[] ch;
        VERBOSE(VB_IMPORTANT,
                QString("myth_system(): Error, CreateProcess() failed because %1")
                .arg(::GetLastError()));
        return MYTHSYSTEM__EXIT__EXECL_ERROR;
    } else {
        delete[] ch;
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
    QCString cstr = file_on_disk.local8Bit();

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
        odst = dst.open(IO_Raw|IO_WriteOnly|IO_Truncate);

    if (!src.isReadable() && !src.isOpen())
        osrc = src.open(IO_Raw|IO_ReadOnly);

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
        ret = mkstemp(ctemplate);
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
    QString filename = QDeepCopy<QString>(fname);
#ifdef USING_MINGW
    struct _stati64 status;
    _stati64(filename.local8Bit(), &status);
    return status.st_size;
#else
    struct stat status;
    stat(filename.local8Bit(), &status);
    // Using off_t requires a lot of 32/64 bit checking.
    // So just get the size in blocks.
    unsigned long long bsize = status.st_blksize;
    unsigned long long nblk  = status.st_blocks;
    unsigned long long approx_size = nblk * bsize;
    return approx_size;
#endif
}
