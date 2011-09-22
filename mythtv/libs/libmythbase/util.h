#ifndef UTIL_H_
#define UTIL_H_

#include <algorithm>
using namespace std;

#include <QStringList>
#include <QDateTime>
#include <QDir>

#include <stdint.h>
#include <time.h>

#include "mythbaseexp.h"
#include "mythtimer.h"
#include "mythsystem.h"
#include "mythevent.h"

class QFile;

 MBASE_PUBLIC  QDateTime mythCurrentDateTime();
 MBASE_PUBLIC  int calc_utc_offset(void);
 MBASE_PUBLIC  QString getTimeZoneID(void);
 MBASE_PUBLIC  bool checkTimeZone(void);
 MBASE_PUBLIC  bool checkTimeZone(const QStringList &master_settings);

 MBASE_PUBLIC  QDateTime MythUTCToLocal(const QDateTime &utc);
 MBASE_PUBLIC  int MythSecsTo(const QDateTime &from, const QDateTime &to);
 MBASE_PUBLIC  QDateTime myth_dt_from_string(const QString &dtstr);
 
 enum DateTimeFormat { kDateFull = 1,
                       kDateShort = 2,
                       kTime = 4,
                       kDateTimeFull = 5, // KDateFull | KTime
                       kDateTimeShort = 6, // KDateShort | KTime
                       // 8 = Placeholder
                       kAddYear = 16, // Add year to string if not included
                       kSimplify = 32
                      };

 MBASE_PUBLIC  QString MythDateTimeToString(const QDateTime &datetime,
                                            uint format = kDateTimeFull);
 MBASE_PUBLIC  QString MythDateToString(const QDate &date,
                                        uint format = kDateFull);
 MBASE_PUBLIC  QString MythTimeToString(const QTime &time,
                                        uint format = kTime);
 
 MBASE_PUBLIC  bool getUptime(time_t &uptime);
 MBASE_PUBLIC  bool getMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);

 MBASE_PUBLIC  bool hasUtf8(const char *str);
#define M_QSTRING_UNICODE(str) hasUtf8(str) ? QString::fromUtf8(str) : str

 MBASE_PUBLIC  bool ping(const QString &host, int timeout);
 MBASE_PUBLIC  bool telnet(const QString &host, int port);

 MBASE_PUBLIC  long long copy(QFile &dst, QFile &src, uint block_size = 0);
 MBASE_PUBLIC  QString createTempFile(QString name_template = "/tmp/mythtv_XXXXXX",
                               bool dir = false);
 MBASE_PUBLIC  void makeFileAccessible(QString filename);

 MBASE_PUBLIC  QString getResponse(const QString &query, const QString &def);
 MBASE_PUBLIC  int     intResponse(const QString &query, int def);

 MBASE_PUBLIC  QString getSymlinkTarget(const QString &start_file,
                                 QStringList   *intermediaries = NULL,
                                 unsigned       maxLinks       = 255);

 MBASE_PUBLIC  void sendPlaybackStart();
 MBASE_PUBLIC  void sendPlaybackEnd();

 MBASE_PUBLIC  void wrapList(QStringList &list, int width);


inline float clamp(float val, float minimum, float maximum)
{
    return min(max(val, minimum), maximum);
}
inline int   clamp(int val, int minimum, int maximum)
{
    return min(max(val, minimum), maximum);
}
inline float lerp(float r, float a, float b)
{
    return ((1.0f - r) * a) + (r * b);
}
inline int   lerp(float r, int a, int b)
{
    return (int) lerp(r, (float) a, (float) b);
}
inline float sq(float a) { return a*a; }
inline int   sq(int   a) { return a*a; }

 MBASE_PUBLIC  bool IsMACAddress(QString MAC);
 MBASE_PUBLIC  bool WakeOnLAN(QString MAC);
 MBASE_PUBLIC  QString FileHash(QString filename);

/// Is A/V Sync destruction daemon is running on this host?
 MBASE_PUBLIC  bool IsPulseAudioRunning(void);

 MBASE_PUBLIC  bool myth_nice(int val);
 MBASE_PUBLIC  void myth_yield(void);
 MBASE_PUBLIC  bool myth_ioprio(int val); // range -1..8, smaller is higher priority

 MBASE_PUBLIC  bool MythRemoveDirectory(QDir &aDir);
 MBASE_PUBLIC  QString &ShellEscape(QString &string);

 MBASE_PUBLIC  void setHttpProxy(void);

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
