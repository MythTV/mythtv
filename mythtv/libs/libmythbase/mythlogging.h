#ifndef MYTHLOGGING_H_
#define MYTHLOGGING_H_

#ifdef __cplusplus
#include <QString>
#include <QThread>
#endif
#include <stdint.h>
#include <time.h>
#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.

#define LOGLINE_MAX 2048

typedef enum
{
    LOG_EMERG = 0,
    LOG_ALERT,
    LOG_CRIT,
    LOG_ERR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG,
    LOG_UNKNOWN
} LogLevel_t;

#ifdef _LogLevelNames_
const char *LogLevelNames[] =
{
    "LOG_EMERG",
    "LOG_ALERT",
    "LOG_CRIT",
    "LOG_ERR",
    "LOG_WARNING",
    "LOG_NOTICE",
    "LOG_INFO",
    "LOG_DEBUG",
    "LOG_UNKNOWN"
};
int LogLevelNameCount = sizeof(LogLevelNames)/sizeof(LogLevelNames[0]);
#else
extern MBASE_PUBLIC char *LogLevelNames[];
extern MBASE_PUBLIC int LogLevelNameCount;
#endif
extern MBASE_PUBLIC LogLevel_t LogLevel;

typedef struct
{
    LogLevel_t          level;
    uint64_t            threadId;
    char               *file;
    int                 line;
    char               *function;
    struct tm           tm;
    uint32_t            usec;
    char               *message;
    char               *threadName;
    int                 registering;
    int                 deregistering;
} LoggingItem_t;


#ifdef __cplusplus
extern "C" {
#endif

#define LogPrintQString(mask, level, ...) \
    LogPrintLineNoArg(mask, (LogLevel_t)level, (char *)__FILE__, __LINE__, \
                      (char *)__FUNCTION__, \
                      (char *)QString(__VA_ARGS__).toLocal8Bit().constData())

#define LogPrint(mask, level, format, ...) \
    LogPrintLine(mask, (LogLevel_t)level, (char *)__FILE__, __LINE__, \
                 (char *)__FUNCTION__, (char *)format, ## __VA_ARGS__)

#define LogPrintNoArg(mask, level, string) \
    LogPrintLineNoArg(mask, (LogLevel_t)level, (char *)__FILE__, __LINE__, \
                      (char *)__FUNCTION__, (char *)string)

/* Define the external prototype */
MBASE_PUBLIC void LogPrintLine( uint32_t mask, LogLevel_t level, char *file, 
                                int line, char *function, char *format, ... );
MBASE_PUBLIC void LogPrintLineNoArg( uint32_t mask, LogLevel_t level, 
                                     char *file, int line, char *function, 
                                     char *message );

#ifdef __cplusplus
}

MBASE_PUBLIC void logStart(QString logfile);
MBASE_PUBLIC void logStop(void);
MBASE_PUBLIC void threadRegister(QString name);
MBASE_PUBLIC void threadDeregister(void);
void LogTimeStamp( time_t *epoch, uint32_t *usec );

typedef union {
    char   *string;
    int     number;
} LoggerHandle_t;


class LoggerBase {
    public:
        LoggerBase(char *string, int number);
        ~LoggerBase();
        virtual bool logmsg(LoggingItem_t *item) = 0;
    protected:
        LoggerHandle_t m_handle;
        bool m_string;
};

class FileLogger : public LoggerBase {
    public:
        FileLogger(char *filename);
        ~FileLogger();
        bool logmsg(LoggingItem_t *item);
    private:
        bool m_opened;
        int  m_fd;
};

class SyslogLogger : public LoggerBase {
    public:
        SyslogLogger(int facility);
        ~SyslogLogger();
        bool logmsg(LoggingItem_t *item);
    private:
        char *m_application;
        bool m_opened;
};

class DatabaseLogger : public LoggerBase {
    public:
        DatabaseLogger(char *table);
        ~DatabaseLogger();
        bool logmsg(LoggingItem_t *item);
    private:
        char *m_host;
        char *m_application;
        char *m_query;
        pid_t m_pid;
        bool m_opened;
};

class LoggerThread : public QThread {
    Q_OBJECT

    public:
        LoggerThread();
        void run(void);
        void stop(void) { aborted = true; };
    private:
        bool aborted;
};
#endif


#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
