#include <qapplication.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "util.h"
#include "mythcontext.h"

#ifndef QWS
extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
}
#endif

#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qfont.h>

#ifdef USE_LIRC
#include "lircevent.h"
#endif

bool connectSocket(QSocketDevice *socket, const QString &host, int port)
{
    QHostAddress hadr;
    hadr.setAddress(host);
    
    socket->setAddressReusable(true);
    bool result = socket->connect(hadr, port);
  
    if (result)
    {
        socket->setReceiveBufferSize(130000); // defaults to 87k 
                                              // max is 131k
        //cout << "connectSocket - buffsize: " 
        //     << socket->receiveBufferSize() << "\n";
    }                                          
    else
    {
        QString errorMsg = "N/A";
        
        if (socket->error() == QSocketDevice::NoError)
            errorMsg = "NoError";
        else if (socket->error() == QSocketDevice::AlreadyBound)    
            errorMsg = "AlreadyBound";
        else if (socket->error() == QSocketDevice::Inaccessible)    
            errorMsg = "Inaccessible";
        else if (socket->error() == QSocketDevice::NoResources)    
            errorMsg = "NoResources";
        else if (socket->error() == QSocketDevice::Bug)    
            errorMsg = "Bug";
        else if (socket->error() == QSocketDevice::Impossible)    
            errorMsg = "Impossible";
        else if (socket->error() == QSocketDevice::NoFiles)    
            errorMsg = "NoFiles";
        else if (socket->error() == QSocketDevice::ConnectionRefused)    
            errorMsg = "ConnectionRefused";
        else if (socket->error() == QSocketDevice::NetworkFailure)    
            errorMsg = "NetworkFailure";
        else if (socket->error() == QSocketDevice::UnknownError)    
            errorMsg = "UnknownError";
        
        VERBOSE(VB_NETWORK, 
             QString("Socket error connecting to host: %1, port: %2, error: %3")
                   .arg(host).arg(port).arg(errorMsg));
    }
    
    return result;
}

bool WriteStringList(QSocketDevice *socket, QStringList &list)
{// QSocketDevice (frontend)
    QString str = list.join("[]:[]");
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
        QString msg = payload;

        if (msg.length() > 58)
        {
            msg.truncate(55);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    unsigned int errorcount = 0;
    bool retval = true;

    while (size > 0)
    {
        int temp = socket->writeBlock(payload.data() + written, size);
        // cerr << "  written: " << temp << endl; //DEBUG
        written += temp;
        size -= temp;
        if (size > 0)
        {
            printf("Partial WriteStringList %u\n", written);

            if (++errorcount > 50)
            {
                retval = false;
                break;
            }  
        }
    }

    return retval;
}

bool ReadStringList(QSocketDevice *socket, QStringList &list, bool quickTimeout)
{// QSocketDevice (frontend)
    list.clear();

    unsigned int errorcount = 0;
    
    if (!socket->isOpen() || socket->error())
        return false;

    while (socket->waitForMore(5) < 8)
    {
        ++errorcount;
        if (!quickTimeout && errorcount > 1500) // ~30 secs
        {
            VERBOSE(VB_NETWORK, "ReadStringList timeout.");
            socket->close();
            return false;
        }
        else if (quickTimeout && errorcount > 150) // ~4 secs
        {
            VERBOSE(VB_NETWORK, "ReadStringList timeout (quick).");
            socket->close();
            return false;
        }
        
        usleep(50);
    }

    QCString sizestr(8 + 1);
    socket->readBlock(sizestr.data(), 8);

    sizestr = sizestr.stripWhiteSpace();
    int size = sizestr.toInt();

    QCString utf8(size + 1);

    int read = 0;
    unsigned int zerocnt = 0;

    while (size > 0)
    {
        int temp = socket->readBlock(utf8.data() + read, size);
        // cerr << "  read: " << temp << endl; //DEBUG
        read += temp;
        size -= temp;
        if (size > 0)
        {
            if (++zerocnt >= 100)
            {
                printf("EOF readStringList %u\n", read);
                break; 
            }
            usleep(50);
            
            if (zerocnt == 5)
            {
                printf("Waiting for data: %u %u\n", read, size);
            }
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    list = QStringList::split("[]:[]", str);

    return true;
}

bool WriteBlock(QSocketDevice *socket, void *data, int len)
{// QSocketDevice (frontend)
    int size = len;
    int written = 0;

    unsigned int errorcount = 0;

    while (size > 0)
    {
        int temp = socket->writeBlock((char *)data + written, size);
        written += temp;
        size -= temp;
        if (size > 0)
        {
            printf("Partial WriteBlock %u\n", written);
            if (++errorcount > 50)
            {
                return false;
            }
            usleep(50);
        }
    }

    return true;
}

int ReadBlock(QSocketDevice *socket, void *data, int maxlen)
{// QSocketDevice (frontend)
    int read = 0;
    int size = maxlen;
    unsigned int zerocnt = 0;

    while (size > 0)
    {
        int temp = socket->readBlock((char *)data + read, size);
        read += temp;
        size -= temp;
        if (size > 0)
        {
            if (++zerocnt >= 100)
            {
                printf("EOF ReadBlock %u\n", read);
                break; 
            }
            usleep(50);
        }
    }
    
    return maxlen;
}


bool WriteStringList(QSocket *socket, QStringList &list)
{// QSocket (backend)
    QString str = list.join("[]:[]");
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
        QString msg = payload;

        if (msg.length() > 58)
        {
            msg.truncate(55);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    unsigned int errorcount = 0;
    bool retval = true;

    while (size > 0)
    {
        qApp->lock();
        int temp = socket->writeBlock(payload.data() + written, size);
        qApp->unlock();
        // cerr << "  written: " << temp << endl; //DEBUG
        written += temp;
        size -= temp;
        if (size > 0)
        {
            printf("Partial WriteStringList %u\n", written);
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

bool ReadStringList(QSocket *socket, QStringList &list)
{// QSocket (backend)
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
        usleep(50);
        qApp->lock();
    }

    QCString sizestr(8 + 1);
    socket->readBlock(sizestr.data(), 8);

    qApp->unlock();

    sizestr = sizestr.stripWhiteSpace();
    int size = sizestr.toInt();

    QCString utf8(size + 1);

    int read = 0;
    unsigned int zerocnt = 0;

    while (size > 0)
    {
        qApp->lock();
        int temp = socket->readBlock(utf8.data() + read, size);
        qApp->unlock();
        // cerr << "  read: " << temp << endl; //DEBUG
        read += temp;
        size -= temp;
        if (size > 0)
        {
            if (++zerocnt >= 100)
            {
                printf("EOF readStringList %u\n", read);
                break; 
            }
            usleep(50);
            qApp->processEvents();

            if (zerocnt == 5)
            {
                printf("Waiting for data: %u %u\n", read, size);
            }
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    list = QStringList::split("[]:[]", str);

    return true;
}

bool WriteBlock(QSocket *socket, void *data, int len)
{// QSocket (backend)
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
            printf("Partial WriteBlock %u\n", written);
            qApp->processEvents();

            if (++errorcount > 50)
                return false;
        }
    }

    qApp->lock();
    if (socket->bytesToWrite() > 0)
        socket->flush();
    qApp->unlock();
    
    while (socket->bytesToWrite() >= (unsigned) written)
    {
        usleep(50000);
    }    

    return true;
}

int ReadBlock(QSocket *socket, void *data, int maxlen)
{// QSocket (backend)
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
                printf("EOF ReadBlock %u\n", read);
                break; 
            }
            usleep(50);
            qApp->processEvents();
        }
    }
    
    return maxlen;
}



void encodeLongLong(QStringList &list, long long num)
{
    list << QString::number((int)(num >> 32));
    list << QString::number((int)(num & 0xffffffffLL));
}

long long decodeLongLong(QStringList &list, int offset)
{
    long long retval = 0;

    int l1 = list[offset].toInt();
    int l2 = list[offset + 1].toInt();
 
    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
} 

#ifndef QWS
void GetMythTVGeometry(Display *dpy, int screen_num, int *x, int *y, 
                       int *w, int *h) 
{
    int event_base, error_base;

    if (XineramaQueryExtension(dpy, &event_base, &error_base) &&
        XineramaIsActive(dpy)) 
    {
        XineramaScreenInfo *xinerama_screens;
        XineramaScreenInfo *screen;
        int nr_xinerama_screens;

        int screen_nr = gContext->GetNumSetting("XineramaScreen", 0);

        xinerama_screens = XineramaQueryScreens(dpy, &nr_xinerama_screens);

        printf("Found %d Xinerama Screens.\n", nr_xinerama_screens);

        if(screen_nr > 0 && screen_nr < nr_xinerama_screens)
        {
            screen = &xinerama_screens[screen_nr];
            printf("Using screen %d, %dx%d+%d+%d\n",
                   screen_nr, screen->width, screen->height, screen->x_org, 
                   screen->y_org );
        } 
        else 
        {
            screen = &xinerama_screens[0];
            printf("Using first Xinerama screen, %dx%d+%d+%d\n",
                   screen->width, screen->height, screen->x_org, screen->y_org);
        }

        *w = screen->width;
        *h = screen->height;
        *x = screen->x_org;
        *y = screen->y_org;

        XFree(xinerama_screens);
    } 
    else 
    {
        *w = DisplayWidth(dpy, screen_num);
        *h = DisplayHeight(dpy, screen_num);
        *x = 0; *y = 0;
    }
}
#endif

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

int myth_system(const QString &command, int flags)
{
#ifdef USE_LIRC
    LircEventLock lirc_lock(!(flags & MYTH_SYSTEM_DONT_BLOCK_LIRC));
#else
    (void)flags;
#endif

    return system(command);
}

QString cutDownString(QString text, QFont *testFont, int maxwidth)
{
    QFontMetrics fm(*testFont);

    int curFontWidth = fm.width(text);
    if (curFontWidth > maxwidth)
    {
        QString testInfo = "";
        curFontWidth = fm.width(testInfo);
        int tmaxwidth = maxwidth - fm.width("LLL");
        int count = 0;

        while (curFontWidth < tmaxwidth)
        {
            testInfo = text.left(count);
            curFontWidth = fm.width(testInfo);
            count = count + 1;
        }

        testInfo = testInfo + "...";
        text = testInfo;
    }

    return text;
}

