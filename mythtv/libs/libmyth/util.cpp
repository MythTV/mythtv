// C++ headers
#include <iostream>
using namespace std;

// C headers
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>

// System specific C headers
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#ifdef linux
#include <sys/vfs.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#else
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#endif

// Qt headers
#include <qapplication.h>
#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qfont.h>

// Myth headers
#include "mythconfig.h"
#include "exitcodes.h"
#include "util.h"
#include "mythcontext.h"

#ifdef CONFIG_DARWIN
#include <mach/mach.h> 
#endif

#ifdef USE_LIRC
#include "lircevent.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenuevent.h"
#endif

#define SOCKET_BUF_SIZE  128000 //< Socket buffer size

/** \fn SocDevErrStr(int)
 *  \brief Returns QString describing a QSocketDevice
 *         error in a human readable form.
 */
QString SocDevErrStr(int error)
{
    QString errorMsg = "N/A";
        
    if (error == QSocketDevice::NoError)
        errorMsg = "NoError";
    else if (error == QSocketDevice::AlreadyBound)    
        errorMsg = "AlreadyBound";
    else if (error == QSocketDevice::Inaccessible)    
        errorMsg = "Inaccessible";
    else if (error == QSocketDevice::NoResources)    
        errorMsg = "NoResources";
    else if (error == QSocketDevice::Bug)    
        errorMsg = "Bug";
    else if (error == QSocketDevice::Impossible)    
        errorMsg = "Impossible";
    else if (error == QSocketDevice::NoFiles)    
        errorMsg = "NoFiles";
    else if (error == QSocketDevice::ConnectionRefused)    
        errorMsg = "ConnectionRefused";
    else if (error == QSocketDevice::NetworkFailure)    
        errorMsg = "NetworkFailure";
    else if (error == QSocketDevice::UnknownError)    
        errorMsg = "UnknownError";
        
   return errorMsg;         
}

/** \fn connectSocket(QSocketDevice*,const QString&,uint)
 *  \brief Connect to backend socket (frontend call).
 */
bool connectSocket(QSocketDevice *socket, const QString &host, uint port)
{
    QHostAddress hadr;
    hadr.setAddress(host);
    
    socket->setAddressReusable(true);
    bool result = socket->connect(hadr, port);
  
    if (result)
    {
        socket->setReceiveBufferSize(SOCKET_BUF_SIZE); // defaults to 87k 
                                                       // max is 131k
        //cout << "connectSocket - buffsize: " 
        //     << socket->receiveBufferSize() << "\n";
    }                                          
    else
    {
        VERBOSE(VB_NETWORK, 
             QString("Socket error connecting to host: %1, port: %2, error: %3")
                   .arg(host).arg(port).arg(SocDevErrStr(socket->error())));
    }
    
    return result;
}

/** \fn WriteStringList(QSocketDevice*,QStringList&)
 *  \brief Write a string list to the socket (frontend call).
 */
bool WriteStringList(QSocketDevice *socket, QStringList &list)
{
    if (!socket->isOpen() || socket->error())
    {
        VERBOSE(VB_IMPORTANT, "WriteStringList: Bad socket");
        return false;
    }    
    
    QString str = list.join("[]:[]");
    QCString utf8 = str.utf8();

    QCString payload;
    payload = payload.setNum(utf8.length());
    payload += "        ";
    payload.truncate(8);
    payload += utf8;
    
    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = QString("write -> %1 %2")
            .arg(socket->socket(), 2).arg(payload);

        if (msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    int btw = payload.length();
    int written = 0;
    
    QTime timer;
    timer.start();

    while (btw > 0)
    {
        int sret = socket->writeBlock(payload.data() + written, btw);
        // cerr << "  written: " << temp << endl; //DEBUG
        if (sret > 0)
        {
            written += sret;
            btw -= sret;
            
            if (btw > 0)
            {
                timer.start();
                VERBOSE(VB_GENERAL,
                        QString("WriteStringList: Partial WriteStringList: %1")
                        .arg(written));
            }                                
        }
        else if (sret < 0 && socket->error() != QSocketDevice::NoError)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("WriteStringList: Error writing string list "
                            "(writeBlock): %1")
                    .arg(SocDevErrStr(socket->error())));
            socket->close();
            return false;
        }
        else
        {
            if (timer.elapsed() > 100000)
            {
                VERBOSE(VB_GENERAL, "WriteStringList timeout");
                return false;
            }  
            
            usleep(500);
        }
    }

    return true;
}

/** \fn ReadStringList(QSocketDevice*,QStringList&,bool)
 *  \brief Read a string list from the socket (frontend call).
 */
bool ReadStringList(QSocketDevice *socket, QStringList &list, bool quickTimeout)
{
    list.clear();

    if (!socket->isOpen() || socket->error())
    {
        VERBOSE(VB_ALL, "ReadStringList: Bad socket");
        return false;
    }    

    QTime timer;
    timer.start();
    int elapsed = 0;

    while (socket->waitForMore(5) < 8)
    {
        elapsed = timer.elapsed();
        if (!quickTimeout && elapsed >= 300000)
        {
            VERBOSE(VB_GENERAL, "ReadStringList timeout.");
            socket->close();
            return false;
        }
        else if (quickTimeout && elapsed >= 20000)
        {
            VERBOSE(VB_GENERAL, "ReadStringList timeout (quick).");
            socket->close();
            return false;
        }
        
        usleep(500);
    }

    QCString sizestr(8 + 1);
    if (socket->readBlock(sizestr.data(), 8) < 0)
    {
        VERBOSE(VB_GENERAL, QString("Error reading stringlist (sizestr): %1")
                                    .arg(SocDevErrStr(socket->error())));
        socket->close();
        return false;
    }

    sizestr = sizestr.stripWhiteSpace();
    int btr = sizestr.toInt();

    if (btr < 1)
    {
        int pending = socket->bytesAvailable();
        QCString dump(pending + 1);
        socket->readBlock(dump.data(), pending);
        VERBOSE(VB_IMPORTANT, QString("Protocol error: '%1' is not a valid "
                                      "size prefix. %2 bytes pending.")
                                      .arg(sizestr).arg(pending));
        return false;
    }

    QCString utf8(btr + 1);

    int read = 0;
    int errmsgtime = 0;
    timer.start();
    
    while (btr > 0)
    {
        int sret = socket->readBlock(utf8.data() + read, btr);
        // cerr << "  read: " << temp << endl; //DEBUG
        if (sret > 0)
        {
            read += sret;
            btr -= sret;
            if (btr > 0)
            {
                usleep(500);
                timer.start();
            }    
        }
        else if (sret < 0 && socket->error() != QSocketDevice::NoError)
        {
            VERBOSE(VB_GENERAL, QString("Error reading stringlist"
                                        " (readBlock): %1")
                                        .arg(SocDevErrStr(socket->error())));
            socket->close();
            return false;
        }
        else
        {
            elapsed = timer.elapsed();
            if (elapsed  > 10000)
            {
                if ((elapsed - errmsgtime) > 10000)
                {
                    errmsgtime = elapsed;
                    VERBOSE(VB_GENERAL, QString("Waiting for data: %1 %2")
                                                .arg(read).arg(btr));
                }                            
            }
            
            if (elapsed > 100000)
            {
                VERBOSE(VB_GENERAL, "ReadStringList timeout. (readBlock)");
                return false;
            }
            
            usleep(500);
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    QCString payload;
    payload = payload.setNum(str.length());
    payload += "        ";
    payload.truncate(8);
    payload += str;
    
    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = QString("read  <- %1 %2").arg(socket->socket(), 2)
                                               .arg(payload);

        if (msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    list = QStringList::split("[]:[]", str, true);

    return true;
}

/** \fn WriteBlock(QSocketDevice*,void*,uint)
 *  \brief Write a block of raw data to the socket (frontend call).
 */
bool WriteBlock(QSocketDevice *socket, void *data, uint len)
{
    
    if (!socket->isOpen() || socket->error())
    {
        VERBOSE(VB_ALL, "WriteBlock: Bad socket");
        return false;
    }    
    
    uint written = 0;
    uint zerocnt = 0;
    
    while (written < len)
    {
        // Push bytes to client. We may push more than the socket buffer,
        // in which case this call will continue writing until all data has
        // been sent. We're counting on the client thread to be pulling data 
        // from the socket, if not we'll timeout and return false.
        
        int btw = len - written >= SOCKET_BUF_SIZE ? 
                                   SOCKET_BUF_SIZE : len - written;
        
        int sret = socket->writeBlock((char *)data + written, btw);
        if (sret > 0)
        {
            zerocnt = 0;
            written += (uint) sret;
            if (written < len)
                usleep(500);
        }
        else if (sret < 0 && socket->error() != QSocketDevice::NoError)
        {
            VERBOSE(VB_IMPORTANT, 
                    QString("Socket write error (writeBlock): %1")
                            .arg(SocDevErrStr(socket->error())));
            socket->close();
            return false;
        }
        else 
        {
            if (++zerocnt > 200)
            {
                VERBOSE(VB_IMPORTANT, "WriteBlock zerocnt timeout");
                return false;
            }    
            usleep(1000); // We're waiting on the client.
        }
    }
    
    return true;
}

/** \fn WriteStringList(QSocketDevice*,QStringList&)
 *  \brief Write a string list to the socket (backend call).
 */
bool WriteStringList(QSocket *socket, QStringList &list)
{
    if (list.size() <= 0)
    {
        VERBOSE(VB_IMPORTANT,
                "WriteStringList: Error, invalid string list.");
        return false;
    }

    if (!socket || socket->state() != QSocket::Connected)
    {
        VERBOSE(VB_IMPORTANT,
                "WriteStringList: Error, writing to unconnected socket.");
        return false;
    }

    QString str = list.join("[]:[]");
    if (str == QString::null)
    {
        VERBOSE(VB_IMPORTANT,
                "WriteStringList: Error, joined null string.");
        return false;
    }

    QCString utf8 = str.utf8();

    int size = utf8.length();

    int written = 0;

    QCString payload;

    payload = payload.setNum(size);
    payload += "        ";
    payload.truncate(8);
    payload += utf8;
    size = payload.length();

    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = QString("write -> %1 %2")
            .arg(socket->socket(), 2).arg(payload);

        if (msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    unsigned int errorcount = 0;
    bool retval = true;

    while (size > 0)
    {
        if (socket->state() != QSocket::Connected)
        {
            VERBOSE(VB_IMPORTANT,
                    "WriteStringList: Error, writing to unconnected socket.");
            return false;
        }

        qApp->lock();
        int temp = socket->writeBlock(payload.data() + written, size);
        qApp->unlock();

        if (temp < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    "WriteStringList: Error, socket->writeBlock() failed.");
            return false;
        }

        // cerr << "  written: " << temp << endl; //DEBUG
        written += temp;
        size -= temp;
        if (size > 0)
        {
            VERBOSE(VB_GENERAL,
                    QString("WriteStringList: Partial WriteStringList %1.")
                    .arg(written));
            qApp->processEvents();

            if (++errorcount > 50)
            {
                retval = false;
                break;
            }  
        }
    }

    qApp->lock();
    if (socket->bytesToWrite() > 0)
        socket->flush();
    qApp->unlock();
    
    return retval;
}

/** \fn ReadStringList(QSocketDevice*,QStringList&)
 *  \brief Read a string list from the socket (backend call).
 */
bool ReadStringList(QSocket *socket, QStringList &list)
{
    list.clear();

    qApp->lock();
    while (socket->waitForMore(5) < 8)
    {
        if (socket->state() != QSocket::Connected)
        {
            // dunno if socket->state() wants the app lock.  be safe for now.
            qApp->unlock();
            return false;
        }

        qApp->unlock();
        usleep(500);
        qApp->lock();
    }

    QCString sizestr(8 + 1);
    socket->readBlock(sizestr.data(), 8);

    qApp->unlock();

    sizestr = sizestr.stripWhiteSpace();
    int size = sizestr.toInt();

    if (size < 1)
    {
        int pending = socket->bytesAvailable();
        QCString dump(pending + 1);
        socket->readBlock(dump.data(), pending);
        VERBOSE(VB_IMPORTANT, QString("Protocol error: '%1' is not a valid "
                                      "size prefix. %2 bytes pending.")
                                      .arg(sizestr).arg(pending));
        return false;
    }

    QCString utf8(size + 1);

    int read = 0;
    unsigned int zerocnt = 0;

    while (size > 0)
    {
        qApp->lock();
        int temp = socket->readBlock(utf8.data() + read, size);
        qApp->unlock();

        if (temp < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    "ReadStringList: Failed to read from socket. Bailing.");
            return false;
        }

        // cerr << "  read: " << temp << endl; //DEBUG
        read += temp;
        size -= temp;
        if (size > 0)
        {
            if (++zerocnt >= 100)
            {
                VERBOSE(VB_IMPORTANT, QString("ReadStringList: Error, EOF %1")
                        .arg(read));
                break; 
            }
            usleep(500);
            qApp->processEvents();

            if (zerocnt == 5)
            {
                VERBOSE(VB_GENERAL,
                        QString("ReadStringList: Waiting for data: %1 %2")
                        .arg(read).arg(size));
            }
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    QCString payload;
    payload = payload.setNum(str.length());
    payload += "        ";
    payload.truncate(8);
    payload += str;
    
    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = QString("read  <- %1 %2")
            .arg(socket->socket(), 2).arg(payload);

        if (msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    list = QStringList::split("[]:[]", str, true);

    return true;
}

/** \fn WriteBlock(QSocket*,void*,uint)
 *  \brief Write a block of raw data to the socket (backend call).
 */
bool WriteBlock(QSocket *socket, void *data, uint len)
{
    int size = len;
    int written = 0;

    unsigned int errorcount = 0;

    while (size > 0)
    {
        qApp->lock();
        int temp = socket->writeBlock((char *)data + written, size);
        qApp->unlock();
        written += temp;
        size -= temp;
        if (size > 0)
        {
            VERBOSE(VB_GENERAL, QString("WriteBlock: Partial write (%1/%2)")
                    .arg(written).arg(len));
            qApp->processEvents();

            if (++errorcount > 50)
                return false;
        }
    }

    qApp->lock();
    if (socket->bytesToWrite() > 0)
        socket->flush();
    
    while (socket->bytesToWrite() >= (unsigned) written)
    {
        socket->flush();
        qApp->unlock();
        usleep(500);
        qApp->lock();
    }    

    qApp->unlock();

    return true;
}

/** \fn ReadBlock(QSocket*,void*,uint)
 *  \brief Read a block of raw data from the socket (backend call).
 */
int ReadBlock(QSocket *socket, void *data, uint maxlen)
{
    int read = 0;
    int size = maxlen;
    unsigned int zerocnt = 0;

    while (size > 0)
    {
        qApp->lock();
        int temp = socket->readBlock((char *)data + read, size);
        qApp->unlock();
        read += temp;
        size -= temp;
        if (size > 0)
        {
            if (++zerocnt >= 100)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("ReadBlock: Error, EOF %1").arg(read));
                break; 
            }
            usleep(500);
            qApp->processEvents();
        }
    }
    
    return maxlen;
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
 *      decodeLongLong(QStringList&,QStringList::iterator&)
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
 *      decodeLongLong(QStringList&,QStringList::iterator&)
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

/** \fn decodeLongLong(QStringList&,QStringList::iterator&)
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
long long decodeLongLong(QStringList &list, QStringList::iterator &it)
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
#ifdef USE_LIRC
    bool lirc_lock_flag = !(flags & MYTH_SYSTEM_DONT_BLOCK_LIRC);
    LircEventLock lirc_lock(lirc_lock_flag && ready_to_lock);
#endif

#ifdef USE_JOYSTICK_MENU
    bool joy_lock_flag = !(flags & MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU);
    JoystickMenuEventLock joystick_lock(joy_lock_flag && ready_to_lock);
#endif

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
        execl("/bin/sh", "sh", "-c", command.ascii(), NULL);
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

        if (waitpid(child, &status, 0) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("myth_system(): Error, waitpid() failed because %1")
                    .arg(strerror(errno)));
            return GENERIC_EXIT_NOT_OK;
        }
        return WEXITSTATUS(status);
    }
    return GENERIC_EXIT_NOT_OK;
}

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
        VERBOSE(VB_ALL, "sysinfo() error");
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
        VERBOSE(VB_ALL, "sysctl() error");
        return false;
    }
    else
        uptime = time(NULL) - bootTime.tv_sec;

#else
    // Hmmm. Not Linux, not FreeBSD or Darwin. What else is there :-)
    VERBOSE(VB_ALL, "Unknown platform. How do I get the uptime?");
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

    if (statfs(cstr, &statbuf) == 0)
    {
        freespace = statbuf.f_bsize * (statbuf.f_bavail >> 10);
        total = statbuf.f_bsize * (statbuf.f_blocks >> 10);
        used  = total - freespace;
    }
    else
    {
        freespace = total = used = -1;
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
    size_t MB = (1024*1024);
#ifdef __linux__
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
