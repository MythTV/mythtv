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

bool WriteStringList(QSocket *socket, QStringList &list)
{
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
    // cerr << payload << endl; //DEBUG
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
        }
    }

    qApp->lock();
    if (socket->bytesToWrite() > 0)
        socket->flush();
    qApp->unlock();

    return true;
}

bool ReadStringList(QSocket *socket, QStringList &list)
{
    list.clear();

    qApp->lock();
    while (socket->waitForMore(5) < 8)
    {
        qApp->unlock();
        usleep(50);
        qApp->lock();
    }

    qApp->unlock();

    QCString sizestr(8 + 1);
    socket->readBlock(sizestr.data(), 8);
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
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    list = QStringList::split("[]:[]", str);

    return true;
}

void WriteBlock(QSocket *socket, void *data, int len)
{
    int size = len;
    int written = 0;

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
        }
    }

    qApp->lock();
    if (socket->bytesToWrite() > 0)
        socket->flush();
    qApp->unlock();
}

int ReadBlock(QSocket *socket, void *data, int maxlen)
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

    if( XineramaQueryExtension(dpy, &event_base, &error_base) &&
        XineramaIsActive(dpy) ) {

        XineramaScreenInfo *xinerama_screens;
        XineramaScreenInfo *screen;
        int nr_xinerama_screens;

        int screen_nr = gContext->GetNumSetting("XineramaScreen", 0);

        xinerama_screens = XineramaQueryScreens(dpy, &nr_xinerama_screens);

        printf("Found %d Xinerama Screens.\n", nr_xinerama_screens);

        if( screen_nr > 0 && screen_nr < nr_xinerama_screens ) {
            screen = &xinerama_screens[screen_nr];
            printf("Using screen %d, %dx%d+%d+%d\n",
                   screen_nr, screen->width, screen->height, screen->x_org, 
                   screen->y_org );
        } else {
            screen = &xinerama_screens[0];
            printf("Using first Xinerama screen, %dx%d+%d+%d\n",
                   screen->width, screen->height, screen->x_org, screen->y_org);
        }

        *w = screen->width;
        *h = screen->height;
        *x = screen->x_org;
        *y = screen->y_org;

        XFree(xinerama_screens);
    } else {
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

void blendImageToPixmap(QPixmap *source, int sourcex, int sourcey,
                        QImage *image, QPainter *destination, int destx,
                        int desty)
{
    int width = image->width();
    int height = image->height();

    if (sourcex + width > source->width())
        width = source->width() - sourcex;
    if (sourcey + height > source->height())
        height = source->height() - sourcey;

    QImage bgimage = source->convertToImage();

    for (int y = sourcey; y < height + sourcey; y++)
    {
        unsigned int *dest = (unsigned int *)bgimage.scanLine(y);
        unsigned int *src = (unsigned int *)image->scanLine(y - sourcey);

        for (int x = sourcex; x < width + sourcex; x++)
        {
            QRgb origcolor = dest[x];
            QRgb destcolor = src[x - sourcex];

            dest[x] = blendColors(origcolor, destcolor, qAlpha(destcolor));
        }
    }

    destination->drawImage(destx, desty, bgimage);
}
