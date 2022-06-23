#ifndef LOGGINGSERVER_H_
#define LOGGINGSERVER_H_

#include <QMutexLocker>
#include <QSocketNotifier>
#include <QMutex>
#include <QQueue>
#include <QElapsedTimer>

#include <cstdint>
#include <ctime>
#include <unistd.h>

#include "mythconfig.h"
#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.
#include "verbosedefs.h"
#include "mthread.h"

class QString;
class MSqlQuery;
class LoggingItem;

/// \brief Base class for the various logging mechanisms
class LoggerBase : public QObject
{
    Q_OBJECT

  public:
    /// \brief LoggerBase Constructor
    explicit LoggerBase(const char *string);
    /// \brief LoggerBase Deconstructor
    ~LoggerBase() override;
    /// \brief Process a log message for the logger instance
    /// \param item LoggingItem containing the log message to process
    virtual bool logmsg(LoggingItem *item) = 0;
    /// \brief Reopen the log file to facilitate log rolling
    virtual void reopen(void) = 0;
    /// \brief Stop logging to the database
    virtual void stopDatabaseAccess(void) { }
  protected:
    QString m_handle {}; ///< semi-opaque handle for identifying instance
};

/// \brief File-based logger - used for logfiles and console
class FileLogger : public LoggerBase
{
    Q_OBJECT

  public:
    explicit FileLogger(const char *filename);
    ~FileLogger() override;
    bool logmsg(LoggingItem *item) override; // LoggerBase
    void reopen(void) override; // LoggerBase
    static FileLogger *create(const QString& filename, QMutex *mutex);
  private:
    bool m_opened {false}; ///< true when the logfile is opened
    int  m_fd     {-1};    ///< contains the file descriptor for the logfile
};

#ifndef _WIN32
/// \brief Syslog-based logger (not available in Windows)
class SyslogLogger : public LoggerBase
{
    Q_OBJECT

  public:
    explicit SyslogLogger(bool open);
    ~SyslogLogger() override;
    bool logmsg(LoggingItem *item) override; // LoggerBase
    /// \brief Unused for this logger.
    void reopen(void) override { } // LoggerBase
    static SyslogLogger *create(QMutex *mutex, bool open = true);
  private:
    bool m_opened {false};  ///< true when syslog channel open.
};
#endif

#if CONFIG_SYSTEMD_JOURNAL
class JournalLogger : public LoggerBase
{
    Q_OBJECT

  public:
    JournalLogger();
    ~JournalLogger() override;
    bool logmsg(LoggingItem *item) override; // LoggerBase
    /// \brief Unused for this logger.
    void reopen(void) override { }; // LoggerBase
    static JournalLogger *create(QMutex *mutex);
};
#endif

class DBLoggerThread;

/// \brief Database logger - logs to the MythTV database
class DatabaseLogger : public LoggerBase
{
    Q_OBJECT

    friend class DBLoggerThread;
  public:
    explicit DatabaseLogger(const char *table);
    ~DatabaseLogger() override;
    bool logmsg(LoggingItem *item) override; // LoggerBase
    void reopen(void) override { }; // LoggerBase
    void stopDatabaseAccess(void) override; // LoggerBase
    static DatabaseLogger *create(const QString& table, QMutex *mutex);
  protected:
    bool logqmsg(MSqlQuery &query, LoggingItem *item);
    void prepare(MSqlQuery &query);
  private:
    bool isDatabaseReady(void);
    static bool tableExists(const QString &table);

    DBLoggerThread *m_thread;   ///< The database queue handling thread
    QString m_query;            ///< The database query to insert log messages
    bool m_opened             {true};  ///< The database is opened
    bool m_loggingTableExists {false}; ///< The desired logging table exists
    QElapsedTimer m_disabledTime;       ///< Elapsed time since the DB logging was disabled
    QElapsedTimer m_errorLoggingTime;   ///< Elapsed time since DB error logging was last done
    static constexpr std::chrono::milliseconds kMinDisabledTime {1s}; ///< Minimum time to disable DB logging
};

using LogMessage = QList<QByteArray>;
using LogMessageList = QList<LogMessage *>;

/// \brief The logging thread that forwards received messages to the consuming
///        loggers via ZeroMQ
class LogForwardThread : public QObject, public MThread
{
    Q_OBJECT

    friend void logSigHup(void);
  public:
    LogForwardThread();
    ~LogForwardThread() override;
    void run(void) override; // MThread
    void stop(void);
  private:
    bool m_aborted {false};          ///< Flag to abort the thread.

    static void forwardMessage(LogMessage *msg);
  signals:
    void incomingSigHup(void);
  protected slots:
    static void handleSigHup(void);
};

MBASE_PUBLIC bool logForwardStart(void);
MBASE_PUBLIC void logForwardStop(void);
MBASE_PUBLIC void logForwardMessage(const QList<QByteArray> &msg);


class QWaitCondition;

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
static constexpr int MAX_QUEUE_LEN { 1000 };
#else
static constexpr ssize_t MAX_QUEUE_LEN { 1000 };
#endif

/// \brief Thread that manages the queueing of logging inserts for the database.
///        The database logging gets throttled if it gets overwhelmed, and also
///        during startup.  Having a second queue allows the rest of the
///        logging to remain in sync and to allow for burstiness in the
///        database due to things like scheduler runs.
class DBLoggerThread : public MThread
{
  public:
    explicit DBLoggerThread(DatabaseLogger *logger);
    ~DBLoggerThread() override;
    DBLoggerThread(const DBLoggerThread &) = delete;            // not copyable
    DBLoggerThread &operator=(const DBLoggerThread &) = delete; // not copyable

    void run(void) override; // MThread
    void stop(void);
    /// \brief Enqueues a LoggingItem onto the queue for the thread to
    ///        consume.
    bool enqueue(LoggingItem *item);

    /// \brief Indicates when the queue is full
    /// \return true when the queue is full
    bool queueFull(void)
    {
        QMutexLocker qLock(&m_queueMutex);
        return (m_queue->size() >= MAX_QUEUE_LEN);
    }
  private:
    DatabaseLogger *m_logger {nullptr};///< The associated logger instance
    QMutex m_queueMutex;               ///< Mutex for protecting the queue
    QQueue<LoggingItem *> *m_queue {nullptr}; ///< Queue of LoggingItems to insert
    QWaitCondition *m_wait {nullptr};  ///< Wait condition used for waiting
                                       ///  for the queue to not be full.
                                       ///  Protected by m_queueMutex
    volatile bool m_aborted {false};   ///< Used during shutdown to indicate
                                       ///  that the thread should stop ASAP.
                                       ///  Protected by m_queueMutex
};

#ifndef _WIN32
MBASE_PUBLIC void logSigHup(void);
#endif

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
