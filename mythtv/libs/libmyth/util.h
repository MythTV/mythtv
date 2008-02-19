#ifndef UTIL_H_
#define UTIL_H_

#include <qstringlist.h>
#include <qdatetime.h>
#include <qcolor.h>

#include <stdint.h>
#include <time.h>

#include "mythexp.h"

#define MYTH_SYSTEM_DONT_BLOCK_LIRC          0x1 //< myth_system() flag to avoid blocking
#define MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU 0x2 //< myth_system() flag to avoid blocking
#define MYTH_SYSTEM_DONT_BLOCK_PARENT        0x4 //< myth_system() flag to avoid blocking

class QPixmap;
class QImage;
class QPainter;
class QFont;
class QFile;

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
MPUBLIC int calc_utc_offset(void);

// This is necessary for GCC 3.3, which has llabs(long long)
// but not abs(long long) or std::llabs(long long)
MPUBLIC inline long long absLongLong(long long  n) { return n >= 0 ? n : -n; }

MPUBLIC void encodeLongLong(QStringList &list, long long num);
MPUBLIC long long decodeLongLong(QStringList &list, uint offset);
MPUBLIC long long decodeLongLong(QStringList &list, QStringList::const_iterator &it);

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

MPUBLIC void myth_eject(void);

MPUBLIC bool hasUtf8(const char *str);
#define M_QSTRING_UNICODE(str) hasUtf8(str) ? QString::fromUtf8(str) : str

MPUBLIC bool ping(const QString &host, int timeout);
MPUBLIC bool telnet(const QString &host, int port);

MPUBLIC long long copy(QFile &dst, QFile &src, uint block_size = 0);
MPUBLIC QString createTempFile(QString name_template = "/tmp/mythtv_XXXXXX",
                               bool dir = false);
MPUBLIC unsigned long long myth_get_approximate_large_file_size(
    const QString &fname);

MPUBLIC double MythGetPixelAspectRatio(void);

// CPU Tick timing function
#ifdef MMX
#ifdef _WIN32
#include "compat.h"
inline void rdtsc(uint64_t &x)
{
    QueryPerformanceCounter((LARGE_INTEGER*)(&x));
}
#else
typedef struct {
    uint a;
    uint b;
} timing_ab_t;
inline void rdtsc(uint64_t &x)
{
    timing_ab_t &y = (timing_ab_t&) x;
    asm("rdtsc \n"
        "mov %%eax, %0 \n"
        "mov %%edx, %1 \n"
        : 
        : "m"(y.a), "m"(y.b)
        : "%eax", "%edx");
}
#endif

#else // if !MMX
inline void rdtsc(uint64_t &x) { x = 0ULL; }
#endif // !MMX

#endif // UTIL_H_
