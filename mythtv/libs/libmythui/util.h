#ifndef UTIL_H_
#define UTIL_H_

#include <qsocket.h>
#include <qstringlist.h>
#include <qcolor.h>

class QPixmap;
class QImage;
class QPainter;
class QFont;

// QSockectDevice (frontend)
bool connectSocket(QSocketDevice *socket, const QString &host, int port);

bool WriteStringList(QSocketDevice *socket, QStringList &list);
bool ReadStringList(QSocketDevice *socket, QStringList &list,
                    bool quickTimeout = false);

bool WriteBlock(QSocketDevice *socket, void *data, int len);
int ReadBlock(QSocketDevice *socket, void *data, int maxlen);

// QSockect (backend)
bool WriteStringList(QSocket *socket, QStringList &list);
bool ReadStringList(QSocket *socket, QStringList &list);

bool WriteBlock(QSocket *socket, void *data, int len);
int ReadBlock(QSocket *socket, void *data, int maxlen);

void encodeLongLong(QStringList &list, long long num);
long long decodeLongLong(QStringList &list, int offset);
long long decodeLongLong(QStringList &list, QStringList::iterator &it);

#if defined(Q_WS_X11)
void GetMythTVGeometry(Display *dpy, int screen_num, int *x, int *y, 
                       int *w, int *h);
#endif

QRgb blendColors(QRgb source, QRgb add, int alpha);

#define MYTH_SYSTEM_DONT_BLOCK_LIRC (1)
int myth_system(const QString &command, int flags = 0);

QString cutDownString(QString text, QFont *testFont, int maxwidth);

long long stringToLongLong(const QString &str);
QString longLongToString(long long ll);
#endif
