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

void loggingRegisterThread(const QString &name);
void loggingDeregisterThread(void);

/// \brief A semi-opaque handle that is used by the various loggers to indicate
///        which instance is being referenced.
typedef union {
    char   *string;
    int     number;
} LoggerHandle_t;

/// \brief Base class for the various logging mechanisms
class LoggerBase : public QObject
{
    Q_OBJECT

  public:
    /// \brief LoggerBase Constructor
    LoggerBase(char *string, int number);
    /// \brief LoggerBase Deconstructor
    virtual ~LoggerBase();
    /// \brief Process a log message for the logger instance
    /// \param item LoggingItem containing the log message to process
    virtual bool logmsg(LoggingItem *item) = 0;
    /// \brief Reopen the log file to facilitate log rolling
    virtual void reopen(void) = 0;
    /// \brief Stop logging to the database
    virtual void stopDatabaseAccess(void) { }
  protected:
    LoggerHandle_t m_handle; ///< semi-opaque handle for identifying instance
    bool m_string;           ///< true if m_handle.string valid, false otherwise
};

/// \brief File-based logger - used for logfiles and console
class FileLogger : public LoggerBase
{
  public:
    FileLogger(char *filename, bool progress, int quiet);
    ~FileLogger();
    bool logmsg(LoggingItem *item);
    void reopen(void);
  private:
    bool m_opened;      ///< true when the logfile is opened
    int  m_fd;          ///< contains the file descriptor for the logfile
    bool m_progress;    ///< show only LOG_ERR and more important (console only)
    int  m_quiet;       ///< silence the console (console only)
};

#ifndef _WIN32
/// \brief Syslog-based logger (not available in Windows)
class SyslogLogger : public LoggerBase
{
  public:
    SyslogLogger(int facility);
    ~SyslogLogger();
    bool logmsg(LoggingItem *item);
    /// \brief Unused for this logger.
    void reopen(void) { };
  private:
    char *m_application;    ///< C-string of the application name
    bool m_opened;          ///< true when syslog channel open.
};
#endif

class DBLoggerThread;

/// \brief Database logger - logs to the MythTV database
class DatabaseLogger : public LoggerBase
{
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
    bool isDatabaseReady(void);
    bool tableExists(const QString &table);

    DBLoggerThread *m_thread;   ///< The database queue handling thread
    QString m_query;            ///< The database query to insert log messages
    bool m_opened;              ///< The database is opened
    bool m_loggingTableExists;  ///< The desired logging table exists
    bool m_disabled;            ///< DB logging is temporarily disabled
    QTime m_disabledTime;       ///< Time when the DB logging was disabled
    QTime m_errorLoggingTime;   ///< Time when DB error logging was last done
    static const int kMinDisabledTime; ///< Minimum time to disable DB logging
                                       ///  (in ms)
};

class QWaitCondition;

/// \brief The logging thread that consumes the logging queue and dispatches
///        each LoggingItem to each logger instance.
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
    QWaitCondition *m_waitNotEmpty; ///< Condition variable for waiting
                                    ///  for the queue to not be empty
                                    ///  Protected by logQueueMutex
    QWaitCondition *m_waitEmpty;    ///< Condition variable for waiting
                                    ///  for the queue to be empty
                                    ///  Protected by logQueueMutex
    bool aborted;                   ///< Flag to abort the thread.
                                    ///  Protected by logQueueMutex
};

#define MAX_QUEUE_LEN 1000

/// \brief Thread that manages the queueing of logging inserts for the database.
///        The database logging gets throttled if it gets overwhelmed, and also
///        during startup.  Having a second queue allows the rest of the
///        logging to remain in sync and to allow for burstiness in the
///        database due to things like scheduler runs.
class DBLoggerThread : public MThread
{
  public:
    DBLoggerThread(DatabaseLogger *logger);
    ~DBLoggerThread();
    void run(void);
    void stop(void);
    /// \brief Enqueues a LoggingItem onto the queue for the thread to 
    ///        consume.
    bool enqueue(LoggingItem *item) 
    { 
        QMutexLocker qLock(&m_queueMutex); 
        if (!aborted)
            m_queue->enqueue(item); 
        return true; 
    }

    /// \brief Indicates when the queue is full
    /// \return true when the queue is full
    bool queueFull(void)
    {
        QMutexLocker qLock(&m_queueMutex); 
        return (m_queue->size() >= MAX_QUEUE_LEN);
    }
  private:
    DatabaseLogger *m_logger;       ///< The associated logger instance
    QMutex m_queueMutex;            ///< Mutex for protecting the queue
    QQueue<LoggingItem *> *m_queue; ///< Queue of LoggingItems to insert
    QWaitCondition *m_wait;         ///< Wait condition used for waiting
                                    ///  for the queue to not be full.
                                    ///  Protected by m_queueMutex
    volatile bool aborted;          ///< Used during shutdown to indicate
                                    ///  that the thread should stop ASAP.
                                    ///  Protected by m_queueMutex
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
