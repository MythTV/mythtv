#ifndef MYTHLOGGING_H_
#define MYTHLOGGING_H_

#ifdef __cplusplus
#include <QString>
#include <QStringList>
#endif
#include <stdint.h>
#include <errno.h>

#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.
#include "verbosedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Helper for checking verbose mask & level outside of LOG macro
#define VERBOSE_LEVEL_NONE        (verboseMask == 0)
#define VERBOSE_LEVEL_CHECK(_MASK_, _LEVEL_) \
    (((verboseMask & (_MASK_)) == (_MASK_)) && logLevel >= (_LEVEL_))

#define VERBOSE please_use_LOG_instead_of_VERBOSE

// There are two LOG macros now.  One for use with Qt/C++, one for use
// without Qt.
//
// Neither of them will lock the calling thread other than momentarily to put
// the log message onto a queue.
#ifdef __cplusplus
#define LOG(_MASK_, _LEVEL_, _STRING_)                                  \
    do {                                                                \
        if (VERBOSE_LEVEL_CHECK((_MASK_), (_LEVEL_)) && ((_LEVEL_)>=0)) \
        {                                                               \
            LogPrintLine(_MASK_, (LogLevel_t)_LEVEL_,                   \
                         __FILE__, __LINE__, __FUNCTION__, 1,           \
                         QString(_STRING_).toLocal8Bit().constData());  \
        }                                                               \
    } while (false)
#else
#define LOG(_MASK_, _LEVEL_, _FORMAT_, ...)                             \
    do {                                                                \
        if (VERBOSE_LEVEL_CHECK((_MASK_), (_LEVEL_)) && ((_LEVEL_)>=0)) \
        {                                                               \
            LogPrintLine(_MASK_, (LogLevel_t)_LEVEL_,                   \
                         __FILE__, __LINE__, __FUNCTION__, 0,           \
                         (const char *)_FORMAT_, ##__VA_ARGS__);        \
        }                                                               \
    } while (0)
#endif

/* Define the external prototype */
MBASE_PUBLIC void LogPrintLine( uint64_t mask, LogLevel_t level, 
                                const char *file, int line, 
                                const char *function, int fromQString,
                                const char *format, ... );

extern MBASE_PUBLIC LogLevel_t logLevel;
extern MBASE_PUBLIC uint64_t   verboseMask;

#ifdef __cplusplus
}

extern MBASE_PUBLIC QStringList logPropagateArgList;
extern MBASE_PUBLIC QString     logPropagateArgs;
extern MBASE_PUBLIC QString     verboseString;

MBASE_PUBLIC void logStart(QString logfile, int progress = 0, int quiet = 0,
                           int facility = 0, LogLevel_t level = LOG_INFO,
                           bool dblog = true, bool propagate = false,
                           bool noserver = false);
MBASE_PUBLIC void logStop(void);
MBASE_PUBLIC void logPropagateCalc(void);
MBASE_PUBLIC bool logPropagateQuiet(void);
MBASE_PUBLIC bool logPropagateNoServer(void);

MBASE_PUBLIC int  syslogGetFacility(QString facility);
MBASE_PUBLIC LogLevel_t logLevelGet(QString level);
MBASE_PUBLIC QString logLevelGetName(LogLevel_t level);
MBASE_PUBLIC int verboseArgParse(QString arg);

/// Verbose helper function for ENO macro
MBASE_PUBLIC QString logStrerror(int errnum);

/// This can be appended to the LOG args with 
/// "+".  Please do not use "<<".  It uses
/// a thread safe version of strerror to produce the
/// string representation of errno and puts it on the
/// next line in the verbose output.
#define ENO (QString("\n\t\t\teno: ") + logStrerror(errno))
#define ENO_STR ENO.toLocal8Bit().constData()
#endif // __cplusplus

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
