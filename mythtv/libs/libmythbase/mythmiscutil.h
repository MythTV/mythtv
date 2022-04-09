#ifndef MYTHMISCUTIL_H_
#define MYTHMISCUTIL_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>

#include <QString>
#include <QDir>

#ifdef _WIN32
    #undef mkdir
#endif

#include "mythbaseexp.h"
#include "mythsystem.h"

MBASE_PUBLIC bool getUptime(std::chrono::seconds &uptime);
MBASE_PUBLIC bool getMemStats(
    int &totalMB, int &freeMB, int &totalVM, int &freeVM);
using loadArray = std::array<double,3>;
MBASE_PUBLIC loadArray getLoadAvgs(void);

MBASE_PUBLIC bool hasUtf8(const char *str);
#define M_QSTRING_UNICODE(str) hasUtf8(str) ? QString::fromUtf8(str) : str

MBASE_PUBLIC bool ping(const QString &host, std::chrono::milliseconds timeout);
MBASE_PUBLIC bool telnet(const QString &host, int port);

namespace MythFile
{
MBASE_PUBLIC long long copy(QFile &dst, QFile &src, uint block_size = 0);
} // namespace MythFile
MBASE_PUBLIC QString createTempFile(
    QString name_template = "/tmp/mythtv_XXXXXX", bool dir = false);
MBASE_PUBLIC bool makeFileAccessible(const QString& filename);

MBASE_PUBLIC QString getResponse(const QString &query, const QString &def);
MBASE_PUBLIC int     intResponse(const QString &query, int def);

MBASE_PUBLIC QString getSymlinkTarget(const QString &start_file,
                                      QStringList   *intermediaries = nullptr,
                                      unsigned       maxLinks       = 255);

MBASE_PUBLIC bool IsMACAddress(const QString& MAC);
MBASE_PUBLIC bool WakeOnLAN(const QString& MAC);
MBASE_PUBLIC bool MythWakeup(const QString &wakeUpCommand,
    uint flags = kMSNone, std::chrono::seconds timeout = 0s);

MBASE_PUBLIC QString FileHash(const QString& filename);

/// Is A/V Sync destruction daemon is running on this host?
MBASE_PUBLIC bool IsPulseAudioRunning(void);

MBASE_PUBLIC bool myth_nice(int val);
MBASE_PUBLIC void myth_yield(void);
/// range -1..8, smaller is higher priority
MBASE_PUBLIC bool myth_ioprio(int val);

MBASE_PUBLIC bool MythRemoveDirectory(QDir &aDir);

MBASE_PUBLIC void setHttpProxy(void);

MBASE_PUBLIC int naturalCompare(const QString &_a, const QString &_b,
                                Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive);

// CPU Tick timing function
#ifdef MMX
#ifdef _WIN32
#include "compat.h"
inline void rdtsc(uint64_t &x)
{
    QueryPerformanceCounter((LARGE_INTEGER*)(&x));
}
#else
struct timing_ab_t {
    uint a;
    uint b;
};
inline void rdtsc(uint64_t &x)
{
    auto &y = (timing_ab_t&) x;
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
