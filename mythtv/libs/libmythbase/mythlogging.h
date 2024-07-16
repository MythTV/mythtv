#ifndef MYTHLOGGING_H_
#define MYTHLOGGING_H_

#include <QString>
#include <QStringList>
#include <cstdint>
#include <cerrno>

#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.
#include "verbosedefs.h"

/* Define the external prototype */
MBASE_PUBLIC void LogPrintLine( uint64_t mask, LogLevel_t level,
                                const char *file, int line,
                                const char *function,
                                QString message);

extern MBASE_PUBLIC LogLevel_t logLevel;
extern MBASE_PUBLIC uint64_t   verboseMask;

extern MBASE_PUBLIC ComponentLogLevelMap componentLogLevel;

extern MBASE_PUBLIC QStringList logPropagateArgList;
extern MBASE_PUBLIC QString     logPropagateArgs;
extern MBASE_PUBLIC QString     verboseString;

// Helper for checking verbose mask & level outside of LOG macro
static inline bool VERBOSE_LEVEL_NONE() { return verboseMask == 0; };
static inline bool VERBOSE_LEVEL_CHECK(uint64_t mask, LogLevel_t level)
{
    if (componentLogLevel.contains(mask))
        return *(componentLogLevel.find(mask)) >= level;
    return (((verboseMask & mask) == mask) && (logLevel >= level));
}

// This doesn't lock the calling thread other than momentarily to put
// the log message onto a queue.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG(_MASK_, _LEVEL_, _QSTRING_)                                 \
    do {                                                                \
        if (VERBOSE_LEVEL_CHECK((_MASK_), (_LEVEL_)) && ((_LEVEL_)>=0)) \
        {                                                               \
            LogPrintLine(_MASK_, _LEVEL_,                               \
                         __FILE__, __LINE__, __FUNCTION__,              \
                         _QSTRING_);                                    \
        }                                                               \
    } while (false)

MBASE_PUBLIC void resetLogging(void);

MBASE_PUBLIC void logStart(const QString& logfile, bool progress = false,
                           int quiet = 0,
                           int facility = 0, LogLevel_t level = LOG_INFO,
                           bool propagate = false,
                           bool loglong = false,
                           bool testHarness = false);
MBASE_PUBLIC void logStop(void);
MBASE_PUBLIC void logPropagateCalc(void);
MBASE_PUBLIC bool logPropagateQuiet(void);

MBASE_PUBLIC int  syslogGetFacility(const QString& facility);
MBASE_PUBLIC LogLevel_t logLevelGet(const QString& level);
MBASE_PUBLIC QString logLevelGetName(LogLevel_t level);
MBASE_PUBLIC int verboseArgParse(const QString& arg);

/// Verbose helper function for ENO macro
MBASE_PUBLIC QString logStrerror(int errnum);

/// This can be appended to the LOG args with
/// "+".  Please do not use "<<".  It uses
/// a thread safe version of strerror to produce the
/// string representation of errno and puts it on the
/// next line in the verbose output.
#define ENO (QString("\n\t\t\teno: ") + logStrerror(errno))
#define ENO_STR ENO.toLocal8Bit().constData()

inline QString pointerToQString(const void *p)
{
    return QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(p),
                    sizeof(void*) * 2, 16, QChar('0'));
}

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
