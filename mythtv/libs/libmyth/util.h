#ifndef UTIL_H_
#define UTIL_H_

#include <qstringlist.h>
#include <qdatetime.h>
#include <qcolor.h>

#include <time.h>

#include "mythexp.h"

#define MYTH_SYSTEM_DONT_BLOCK_LIRC          0x1 //< myth_system() flag to avoid blocking
#define MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU 0x2 //< myth_system() flag to avoid blocking

class QPixmap;
class QImage;
class QPainter;
class QFont;

class MPUBLIC MythTimer
{
  public:
    MythTimer() {}

    void start() { m_timer.start(); }
    int restart() { int ret = elapsed(); 
                    m_timer.restart(); 
                    return ret; 
                  }
    int elapsed() { int ret = m_timer.elapsed();
                    if (ret > 86300000) { ret = 0;  m_timer.restart(); }
                    return ret;
                  }

    void addMSecs(int ms) { m_timer.addMSecs(ms); }

  private:
    QTime m_timer;
};

MPUBLIC QDateTime mythCurrentDateTime();

// This is necessary for GCC 3.3, which has llabs(long long)
// but not abs(long long) or std::llabs(long long)
MPUBLIC inline long long absLongLong(long long  n) { return n >= 0 ? n : -n; }

MPUBLIC void encodeLongLong(QStringList &list, long long num);
MPUBLIC long long decodeLongLong(QStringList &list, uint offset);
MPUBLIC long long decodeLongLong(QStringList &list, QStringList::iterator &it);

MPUBLIC QRgb blendColors(QRgb source, QRgb add, int alpha);

MPUBLIC uint myth_system(const QString &command, int flags = 0);

MPUBLIC QString cutDownString(const QString &text, QFont *testFont, uint maxwidth);

MPUBLIC QDateTime MythUTCToLocal(const QDateTime &utc);
MPUBLIC int MythSecsTo(const QDateTime &from, const QDateTime &to);

MPUBLIC long long stringToLongLong(const QString &str);
MPUBLIC QString longLongToString(long long ll);

MPUBLIC long long getDiskSpace(const QString&,long long&,long long&);
MPUBLIC bool getUptime(time_t &uptime);
MPUBLIC bool getMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);

#endif // UTIL_H_
