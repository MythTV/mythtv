#ifndef LOGGING_H_
#define LOGGING_H_

#ifdef __cplusplus
#include <QString>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QRegExp>
#endif
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.
#include "verbosedefs.h"
#include "mthread.h"

#define LOGLINE_MAX 2048

typedef struct
{
    LogLevel_t          level;
    uint64_t            threadId;
    const char         *file;
    int                 line;
    const char         *function;
    struct tm           tm;
    uint32_t            usec;
    char               *message;
    char               *threadName;
    int                 registering;
    int                 deregistering;
    int                 refcount;
    void               *refmutex;
} LoggingItem_t;

typedef union {
    char   *string;
    int     number;
} LoggerHandle_t;

class LoggerBase : public QObject {
    Q_OBJECT

    public:
        LoggerBase(char *string, int number);
        ~LoggerBase();
        virtual bool logmsg(LoggingItem_t *item) = 0;
        virtual void reopen(void) = 0;
    protected:
        LoggerHandle_t m_handle;
        bool m_string;
};

class FileLogger : public LoggerBase {
    public:
        FileLogger(char *filename, bool progress, int quiet);
        ~FileLogger();
        bool logmsg(LoggingItem_t *item);
        void reopen(void);
    private:
        bool m_opened;
        int  m_fd;
        bool m_progress;
        int  m_quiet;
};

#ifndef _WIN32
class SyslogLogger : public LoggerBase {
    public:
        SyslogLogger(int facility);
        ~SyslogLogger();
        bool logmsg(LoggingItem_t *item);
        void reopen(void) { };
    private:
        char *m_application;
        bool m_opened;
};
#endif

class DBLoggerThread;

class DatabaseLogger : public LoggerBase {
    friend class DBLoggerThread;
    public:
        DatabaseLogger(char *table);
        ~DatabaseLogger();
        bool logmsg(LoggingItem_t *item);
        void reopen(void) { };
    protected:
        bool logqmsg(LoggingItem_t *item);
    private:
        bool isDatabaseReady();
        bool tableExists(const QString &table);

        DBLoggerThread *m_thread;
        char *m_host;
        char *m_application;
        char *m_query;
        pid_t m_pid;
        bool m_opened;
        bool m_loggingTableExists;
        bool m_disabled;
};

class QWaitCondition;
class LoggerThread : public MThread
{
    public:
        LoggerThread();
        ~LoggerThread();
        void run(void);
        void stop(void);
    private:
        QWaitCondition *m_wait; // protected by logQueueMutex
        bool aborted; // protected by logQueueMutex
};

#define MAX_QUEUE_LEN 1000

class DBLoggerThread : public MThread
{
    public:
        DBLoggerThread(DatabaseLogger *logger);
        ~DBLoggerThread();
        void run(void);
        void stop(void);
        bool enqueue(LoggingItem_t *item) 
        { 
            QMutexLocker qLock(&m_queueMutex); 
            m_queue->enqueue(item); 
            return true; 
        }
        bool queueFull(void)
        {
            QMutexLocker qLock(&m_queueMutex); 
            return (m_queue->size() >= MAX_QUEUE_LEN);
        }
    private:
        DatabaseLogger *m_logger;
        QMutex m_queueMutex;
        QQueue<LoggingItem_t *> *m_queue;
        QWaitCondition *m_wait; // protected by m_queueMutex
        bool aborted; // protected by m_queueMutex
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
