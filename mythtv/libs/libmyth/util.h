#ifndef UTIL_H_
#define UTIL_H_

#include <qsocket.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <qcolor.h>

#include <time.h>

class QPixmap;
class QImage;
class QPainter;
class QFont;

QString SocDevErrStr(int error);

// QSockectDevice (frontend)
bool connectSocket(QSocketDevice *socket, const QString &host, uint port);

bool WriteStringList(QSocketDevice *socket, QStringList &list);
bool ReadStringList(QSocketDevice *socket, QStringList &list,
                    bool quickTimeout = false);

bool WriteBlock(QSocketDevice *socket, void *data, uint len);

// QSockect (backend)
bool WriteStringList(QSocket *socket, QStringList &list);
bool ReadStringList(QSocket *socket, QStringList &list);

bool WriteBlock(QSocket *socket, void *data, uint len);
int ReadBlock(QSocket *socket, void *data, uint maxlen);

void encodeLongLong(QStringList &list, long long num);
long long decodeLongLong(QStringList &list, uint offset);
long long decodeLongLong(QStringList &list, QStringList::iterator &it);

QRgb blendColors(QRgb source, QRgb add, int alpha);

#define MYTH_SYSTEM_DONT_BLOCK_LIRC (1)
#define MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU (2)
uint myth_system(const QString &command, int flags = 0);

QString cutDownString(const QString &text, QFont *testFont, uint maxwidth);

QDateTime MythUTCToLocal(const QDateTime &utc);
int MythSecsTo(const QDateTime &from, const QDateTime &to);

long long stringToLongLong(const QString &str);
QString longLongToString(long long ll);

bool diskUsage(const char *path, double &total, double &used, double &free);
bool getUptime(time_t &uptime);
bool getMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);
#endif
