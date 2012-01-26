#ifndef LOGGING_H_
#define LOGGING_H_

#include <QMutexLocker>
#include <QMutex>
#include <QQueue>
#include <QTime>

#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.
#include "verbosedefs.h"
#include "mthread.h"

#define LOGLINE_MAX (2048-120)

class QString;
class MSqlQuery;
class LoggingItem;

typedef union {
    char   *string;
    int     number;
} LoggerHandle_t;

class LoggerBase : public QObject {
    Q_OBJECT

    public:
        LoggerBase(char *string, int number);
        virtual ~LoggerBase();
        virtual bool logmsg(LoggingItem *item) = 0;
        virtual void reopen(void) = 0;
        virtual void stopDatabaseAccess(void) { }
    protected:
        LoggerHandle_t m_handle;
        bool m_string;
};

class FileLogger : public LoggerBase {
    public:
        FileLogger(char *filename, bool progress, int quiet);
        ~FileLogger();
        bool logmsg(LoggingItem *item);
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
        bool logmsg(LoggingItem *item);
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
        bool logmsg(LoggingItem *item);
        void reopen(void) { };
        virtual void stopDatabaseAccess(void);
    protected:
        bool logqmsg(MSqlQuery &query, LoggingItem *item);
        void prepare(MSqlQuery &query);
    private:
        bool isDatabaseReady();
        bool tableExists(const QString &table);

        DBLoggerThread *m_thread;
        QString m_query;
        bool m_opened;
        bool m_loggingTableExists;
        bool m_disabled; // only accessed from logmsg
        QTime m_disabledTime; // only accessed from logmsg
        QTime m_errorLoggingTime; // only accessed from logqmsg
        static const int kMinDisabledTime;
};

class QWaitCondition;
class LoggerThread : public MThread
{
    public:
        LoggerThread();
        ~LoggerThread();
        void run(void);
        void stop(void);
        bool flush(int timeoutMS = 200000);
        void handleItem(LoggingItem *item);
    private:
        QWaitCondition *m_waitNotEmpty; // protected by logQueueMutex
        QWaitCondition *m_waitEmpty; // protected by logQueueMutex
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
        bool enqueue(LoggingItem *item) 
        { 
            QMutexLocker qLock(&m_queueMutex); 
            if (!aborted)
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
        QQueue<LoggingItem *> *m_queue;
        QWaitCondition *m_wait; // protected by m_queueMutex
        volatile bool aborted; // protected by m_queueMutex
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
