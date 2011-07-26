#ifndef MYTHLOGGING_H_
#define MYTHLOGGING_H_

#ifdef __cplusplus
#include <QString>
#endif
#include <stdint.h>
#include <errno.h>

#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.
#include "verbosedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

// There are two LOG macros now.  One for use with Qt/C++, one for use
// without Qt.
//
// Neither of them will lock the calling thread other than momentarily to put
// the log message onto a queue.
#ifdef __cplusplus
#define LOG(mask, level, string) \
    LogPrintLine(mask, (LogLevel_t)level, __FILE__, __LINE__, __FUNCTION__, \
                 1, QString(string).toLocal8Bit().constData())
#else
#define LOG(mask, level, format, ...) \
    LogPrintLine(mask, (LogLevel_t)level, __FILE__, __LINE__, __FUNCTION__, \
                 0, (const char *)format, ##__VA_ARGS__)
#endif

// Helper for checking verbose mask & level outside of LOG macro
#define VERBOSE_LEVEL_NONE        (verboseMask == 0)
#define VERBOSE_LEVEL_CHECK(mask, level) \
   (((verboseMask & (mask)) == (mask)) && logLevel >= (level))

MBASE_PUBLIC void VERBOSE(uint64_t mask, ...)
    MERROR("VERBOSE is gone, use LOG");

/* Define the external prototype */
MBASE_PUBLIC void LogPrintLine( uint64_t mask, LogLevel_t level, 
                                const char *file, int line, 
                                const char *function, int fromQString,
                                const char *format, ... );

extern MBASE_PUBLIC LogLevel_t logLevel;
extern MBASE_PUBLIC uint64_t   verboseMask;

#ifdef __cplusplus
}

extern MBASE_PUBLIC QString    logPropagateArgs;
extern MBASE_PUBLIC QString    verboseString;

MBASE_PUBLIC void logStart(QString logfile, int progress = 0, int quiet = 0,
                           int facility = 0, LogLevel_t level = LOG_INFO,
                           bool dblog = true, bool propagate = false);
MBASE_PUBLIC void logStop(void);
MBASE_PUBLIC void logPropagateCalc(void);
MBASE_PUBLIC bool logPropagateQuiet(void);

MBASE_PUBLIC void threadRegister(QString name);
MBASE_PUBLIC void threadDeregister(void);
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
