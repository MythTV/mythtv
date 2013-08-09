#ifndef MYTHMISCUTIL_H_
#define MYTHMISCUTIL_H_

#include <stdint.h>

#include <algorithm>
using namespace std;

#include <QString>
#include <QDir>

#include "mythbaseexp.h"

class QStringList;
class QFile;

MBASE_PUBLIC bool getUptime(time_t &uptime);
MBASE_PUBLIC bool getMemStats(
    int &totalMB, int &freeMB, int &totalVM, int &freeVM);

MBASE_PUBLIC bool hasUtf8(const char *str);
#define M_QSTRING_UNICODE(str) hasUtf8(str) ? QString::fromUtf8(str) : str

MBASE_PUBLIC bool ping(const QString &host, int timeout);
MBASE_PUBLIC bool telnet(const QString &host, int port);

MBASE_PUBLIC long long copy(QFile &dst, QFile &src, uint block_size = 0);
MBASE_PUBLIC QString createTempFile(
    QString name_template = "/tmp/mythtv_XXXXXX", bool dir = false);
MBASE_PUBLIC bool makeFileAccessible(QString filename);

MBASE_PUBLIC QString getResponse(const QString &query, const QString &def);
MBASE_PUBLIC int     intResponse(const QString &query, int def);

MBASE_PUBLIC QString getSymlinkTarget(const QString &start_file,
                                      QStringList   *intermediaries = NULL,
                                      unsigned       maxLinks       = 255);

MBASE_PUBLIC void wrapList(QStringList &list, int width);

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

static inline QString xml_bool_to_string(bool val)
{
    return (val) ? "true" : "false";
}

MBASE_PUBLIC QString xml_indent(uint level);

MBASE_PUBLIC bool IsMACAddress(QString MAC);
MBASE_PUBLIC bool WakeOnLAN(QString MAC);
MBASE_PUBLIC QString FileHash(QString filename);

/// Is A/V Sync destruction daemon is running on this host?
MBASE_PUBLIC bool IsPulseAudioRunning(void);

MBASE_PUBLIC bool myth_nice(int val);
MBASE_PUBLIC void myth_yield(void);
/// range -1..8, smaller is higher priority
MBASE_PUBLIC bool myth_ioprio(int val);

MBASE_PUBLIC bool MythRemoveDirectory(QDir &aDir);

MBASE_PUBLIC void setHttpProxy(void);

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

#endif // MYTHMISCUTIL_H_
