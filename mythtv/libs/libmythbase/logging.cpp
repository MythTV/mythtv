#include <QAtomicInt>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QList>
#include <QQueue>
#include <QHash>
#include <QCoreApplication>
#include <QFileInfo>
#include <QStringList>
#include <QMap>
#include <QRegExp>
#include <iostream>

using namespace std;

#include "mythlogging.h"
#include "logging.h"
#include "mythconfig.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "dbutil.h"
#include "exitcodes.h"
#include "compat.h"

#include <stdlib.h>
#define SYSLOG_NAMES
#ifndef _WIN32
#include <syslog.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#if HAVE_GETTIMEOFDAY
#include <sys/time.h>
#endif
#include <signal.h>

// Various ways to get to thread's tid
#if defined(linux)
#include <sys/syscall.h>
#elif defined(__FreeBSD__)
extern "C" {
#include <sys/ucontext.h>
#include <sys/thr.h>
}
#elif CONFIG_DARWIN
#include <mach/mach.h>
#endif

QMutex                  loggerListMutex;
QList<LoggerBase *>     loggerList;

QMutex                  logQueueMutex;
QQueue<LoggingItem *> logQueue;
QRegExp                 logRegExp = QRegExp("[%]{1,2}");

QMutex                  logThreadMutex;
QHash<uint64_t, char *> logThreadHash;

QMutex                   logThreadTidMutex;
QHash<uint64_t, int64_t> logThreadTidHash;

LoggerThread           *logThread = NULL;
bool                    logThreadFinished = false;
bool                    debugRegistration = false;

typedef struct {
    bool    propagate;
    int     quiet;
    int     facility;
    bool    dblog;
    QString path;
} LogPropagateOpts;

LogPropagateOpts        logPropagateOpts;
QString                 logPropagateArgs;

#define TIMESTAMP_MAX 30
#define MAX_STRING_LENGTH (LOGLINE_MAX+120)

LogLevel_t logLevel = (LogLevel_t)LOG_INFO;

typedef struct {
    uint64_t    mask;
    QString     name;
    bool        additive;
    QString     helpText;
} VerboseDef;
typedef QMap<QString, VerboseDef *> VerboseMap;

typedef struct {
    int         value;
    QString     name;
    char        shortname;
} LoglevelDef;
typedef QMap<int, LoglevelDef *> LoglevelMap;

bool verboseInitialized = false;
VerboseMap verboseMap;
QMutex verboseMapMutex;

LoglevelMap loglevelMap;
QMutex loglevelMapMutex;

const uint64_t verboseDefaultInt = VB_GENERAL;
const char    *verboseDefaultStr = " general";

uint64_t verboseMask = verboseDefaultInt;
QString verboseString = QString(verboseDefaultStr);

uint64_t     userDefaultValueInt = verboseDefaultInt;
QString      userDefaultValueStr = QString(verboseDefaultStr);
bool         haveUserDefaultValues = false;

void verboseAdd(uint64_t mask, QString name, bool additive, QString helptext);
void loglevelAdd(int value, QString name, char shortname);
void verboseInit(void);
void verboseHelp(void);

void LogTimeStamp( struct tm *tm, uint32_t *usec );
char *getThreadName( LoggingItem *item );
int64_t getThreadTid( LoggingItem *item );
void setThreadTid( LoggingItem *item );
static LoggingItem *createItem(
    const char *, const char *, int, LogLevel_t, int);
static void deleteItem(LoggingItem *item);
#ifndef _WIN32
void logSighup( int signum, siginfo_t *info, void *secret );
#endif

typedef enum {
    kMessage       = 0x01,
    kRegistering   = 0x02,
    kDeregistering = 0x04,
    kFlush         = 0x08,
    kStandardIO    = 0x10,
} LoggingType;

class LoggingItem
{
  public:
    LoggingItem(const char *_file, const char *_function,
                int _line, LogLevel_t _level, int _type) :
        threadId((uint64_t)(QThread::currentThreadId())),
        line(_line), type(_type), level(_level), file(_file),
        function(_function), threadName(NULL)
    {
        LogTimeStamp(&tm, &usec);
        message[0]='\0';
        message[LOGLINE_MAX]='\0';
        setThreadTid(this);
        refcount.ref();
    }

    QAtomicInt          refcount;
    uint64_t            threadId;
    uint32_t            usec;
    int                 line;
    int                 type;
    LogLevel_t          level;
    struct tm           tm;
    const char         *file;
    const char         *function;
    char               *threadName;
    char                message[LOGLINE_MAX+1];
};

/// \brief LoggerBase class constructor.  Adds the new logger instance to the
///        loggerList.
/// \param string a C-string of the handle for this instance (NULL if unused)
/// \param number an integer for the handle for this instance
LoggerBase::LoggerBase(char *string, int number)
{
    QMutexLocker locker(&loggerListMutex);
    if (string)
    {
        m_handle.string = strdup(string);
        m_string = true;
    }
    else
    {
        m_handle.number = number;
        m_string = false;
    }
    loggerList.append(this);
}

/// \brief LoggerBase deconstructor.  Removes the logger instance from the
///        loggerList.
LoggerBase::~LoggerBase()
{
    QMutexLocker locker(&loggerListMutex);

    QList<LoggerBase *>::iterator it;

    for (it = loggerList.begin(); it != loggerList.end(); ++it)
    {
        if( *it == this )
        {
            loggerList.erase(it);
            break;
        }
    }

    if (m_string)
        free(m_handle.string);
}


/// \brief FileLogger constructor
/// \param filename Filename of the logfile.  "-" for console logging
/// \param progress Show only messages of LOG_ERR and more important, as the
///                 console will be used for progress updates.  Console only.
/// \param quiet    Do not log to the console.  Used for daemon mode.
///                 Console only.
FileLogger::FileLogger(char *filename, bool progress, int quiet) :
        LoggerBase(filename, 0), m_opened(false), m_fd(-1),
        m_progress(progress), m_quiet(quiet)
{
    if( !strcmp(filename, "-") )
    {
        m_opened = true;
        m_fd = 1;
        LOG(VB_GENERAL, LOG_INFO, "Added logging to the console");
    }
    else
    {
        m_progress = false;
        m_quiet = 0;
        m_fd = open(filename, O_WRONLY|O_CREAT|O_APPEND, 0664);
        m_opened = (m_fd != -1);
        LOG(VB_GENERAL, LOG_INFO, QString("Added logging to %1")
                 .arg(filename));
    }
}

/// \brief FileLogger deconstructor - close the logfile
FileLogger::~FileLogger()
{
    if( m_opened )
    {
        if( m_fd != 1 )
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Removed logging to %1")
                     .arg(m_handle.string));
            close( m_fd );
        }
        else
            LOG(VB_GENERAL, LOG_INFO, "Removed logging to the console");
    }
}

/// \brief Reopen the logfile after a SIGHUP.  Log files only (no console).
///        This allows for logrollers to be used.
void FileLogger::reopen(void)
{
    char *filename = m_handle.string;

    // Skip console
    if( !strcmp(filename, "-") )
        return;

    close(m_fd);

    m_fd = open(filename, O_WRONLY|O_CREAT|O_APPEND, 0664);
    m_opened = (m_fd != -1);
    LOG(VB_GENERAL, LOG_INFO, QString("Rolled logging on %1")
             .arg(filename));
}

/// \brief Process a log message, writing to the logfile
/// \param item LoggingItem containing the log message to process
bool FileLogger::logmsg(LoggingItem *item)
{
    char                line[MAX_STRING_LENGTH];
    char                usPart[9];
    char                timestamp[TIMESTAMP_MAX];
    char               *threadName = NULL;
    pid_t               pid = getpid();

    if (!m_opened || m_quiet || (m_progress && item->level > LOG_ERR))
        return false;

    item->refcount.ref();

    strftime( timestamp, TIMESTAMP_MAX-8, "%Y-%m-%d %H:%M:%S",
              (const struct tm *)&item->tm );
    snprintf( usPart, 9, ".%06d", (int)(item->usec) );
    strcat( timestamp, usPart );
    char shortname;

    {
        QMutexLocker locker(&loglevelMapMutex);
        LoglevelMap::iterator it = loglevelMap.find(item->level);
        if (it == loglevelMap.end())
            shortname = '-';
        else
            shortname = (*it)->shortname;
    }

    if (item->type & kStandardIO)
    {
        snprintf( line, MAX_STRING_LENGTH, "%s", item->message );
    }
    else if (m_fd == 1)
    {
        // Stdout
        snprintf( line, MAX_STRING_LENGTH, "%s %c  %s\n", timestamp,
                  shortname, item->message );
    }
    else
    {
        threadName = getThreadName(item);
        pid_t tid = getThreadTid(item);

        if( tid )
            snprintf( line, MAX_STRING_LENGTH, 
                      "%s %c [%d/%d] %s %s:%d (%s) - %s\n",
                      timestamp, shortname, pid, tid, threadName, item->file,
                      item->line, item->function, item->message );
        else
            snprintf( line, MAX_STRING_LENGTH,
                      "%s %c [%d] %s %s:%d (%s) - %s\n",
                      timestamp, shortname, pid, threadName, item->file,
                      item->line, item->function, item->message );
    }

    int fd = (item->type & kStandardIO) ? 1 : m_fd;
    int result = write( fd, line, strlen(line) );

    deleteItem(item);

    if( result == -1 )
    {
        LOG(VB_GENERAL, LOG_ERR,
                 QString("Closed Log output on fd %1 due to errors").arg(m_fd));
        m_opened = false;
        if( m_fd != 1 )
            close( m_fd );
        return false;
    }
    return true;
}


#ifndef _WIN32
/// \brief SyslogLogger constructor
/// \param facility Syslog facility to use in logging
SyslogLogger::SyslogLogger(int facility) : LoggerBase(NULL, facility),
                                           m_opened(false)
{
    CODE *name;

    m_application = strdup((char *)QCoreApplication::applicationName()
                           .toLocal8Bit().constData());

    openlog( m_application, LOG_NDELAY | LOG_PID, facility );
    m_opened = true;

    for (name = &facilitynames[0];
         name->c_name && name->c_val != facility; name++);

    LOG(VB_GENERAL, LOG_INFO, QString("Added syslogging to facility %1")
             .arg(name->c_name));
}

/// \brief SyslogLogger deconstructor.
SyslogLogger::~SyslogLogger()
{
    LOG(VB_GENERAL, LOG_INFO, "Removing syslogging");
    free(m_application);
    closelog();
}


/// \brief Process a log message, logging to syslog
/// \param item LoggingItem containing the log message to process
bool SyslogLogger::logmsg(LoggingItem *item)
{
    if (!m_opened)
        return false;

    char shortname;

    {
        QMutexLocker locker(&loglevelMapMutex);
        LoglevelMap::iterator it = loglevelMap.find(item->level);
        if (it == loglevelMap.end())
            shortname = '-';
        else
            shortname = (*it)->shortname;
    }

    char *threadName = getThreadName(item);

    syslog( item->level, "%c %s %s:%d (%s) %s", shortname, threadName,
            item->file, item->line, item->function, item->message );

    return true;
}
#endif

const int DatabaseLogger::kMinDisabledTime = 1000;

/// \brief DatabaseLogger constructor
/// \param table C-string of the database table to log to
DatabaseLogger::DatabaseLogger(char *table) : LoggerBase(table, 0),
                                              m_opened(false),
                                              m_loggingTableExists(false)
{
    m_query = QString(
        "INSERT INTO %1 "
        "    (host, application, pid, tid, thread, filename, "
        "     line, function, msgtime, level, message) "
        "VALUES (:HOST, :APP, :PID, :TID, :THREAD, :FILENAME, "
        "        :LINE, :FUNCTION, :MSGTIME, :LEVEL, :MESSAGE)")
        .arg(m_handle.string);

    LOG(VB_GENERAL, LOG_INFO, QString("Added database logging to table %1")
        .arg(m_handle.string));

    m_thread = new DBLoggerThread(this);
    m_thread->start();

    m_opened = true;
    m_disabled = false;
}

/// \brief DatabaseLogger deconstructor
DatabaseLogger::~DatabaseLogger()
{
    LOG(VB_GENERAL, LOG_INFO, "Removing database logging");

    stopDatabaseAccess();
}

/// \brief Stop logging to the database and wait for the thread to stop.
void DatabaseLogger::stopDatabaseAccess(void)
{
    if( m_thread )
    {
        m_thread->stop();
        m_thread->wait();
        delete m_thread;
        m_thread = NULL;
    }
}

/// \brief Process a log message, queuing it for logging to the database
/// \param item LoggingItem containing the log message to process
bool DatabaseLogger::logmsg(LoggingItem *item)
{
    if (!m_thread)
        return false;

    if (!m_thread->isRunning())
    {
        m_disabled = true;
        m_disabledTime.start();
    }

    if (!m_disabled && m_thread->queueFull())
    {
        m_disabled = true;
        m_disabledTime.start();
        LOG(VB_GENERAL, LOG_CRIT,
            "Disabling DB Logging: too many messages queued");
        return false;
    }

    if (m_disabled && m_disabledTime.elapsed() > kMinDisabledTime)
    {
        if (isDatabaseReady() && !m_thread->queueFull())
        {
            m_disabled = false;
            LOG(VB_GENERAL, LOG_CRIT, "Reenabling DB Logging");
        }
    }

    if (m_disabled)
        return false;

    item->refcount.ref();
    m_thread->enqueue(item);
    return true;
}

/// \brief Actually insert a log message from the queue into the database
/// \param query    The database insert query to use
/// \param item     LoggingItem containing the log message to insert
bool DatabaseLogger::logqmsg(MSqlQuery &query, LoggingItem *item)
{
    char        timestamp[TIMESTAMP_MAX];
    char       *threadName = getThreadName(item);
    pid_t       tid        = getThreadTid(item);

    strftime( timestamp, TIMESTAMP_MAX-8, "%Y-%m-%d %H:%M:%S",
              (const struct tm *)&item->tm );

    query.bindValue(":TID",         tid);
    query.bindValue(":THREAD",      threadName);
    query.bindValue(":FILENAME",    item->file);
    query.bindValue(":LINE",        item->line);
    query.bindValue(":FUNCTION",    item->function);
    query.bindValue(":MSGTIME",     timestamp);
    query.bindValue(":LEVEL",       item->level);
    query.bindValue(":MESSAGE",     item->message);

    if (!query.exec())
    {
        // Suppress Driver not loaded errors that occur at startup.
        // and suppress additional errors for one second after the
        // previous error (to avoid spamming the log).
        QSqlError err = query.lastError();
        if ((err.type() != 1 || err.number() != -1) &&
            (!m_errorLoggingTime.isValid() ||
             (m_errorLoggingTime.elapsed() > 1000)))
        {
            MythDB::DBError("DBLogging", query);
            m_errorLoggingTime.start();
        }
        return false;
    }

    deleteItem(item);

    return true;
}

/// \brief Prepare the database query for use, and bind constant values to it.
/// \param query    The database query to prepare
void DatabaseLogger::prepare(MSqlQuery &query)
{
    query.prepare(m_query);
    query.bindValue(":HOST", gCoreContext->GetHostName());
    query.bindValue(":APP", QCoreApplication::applicationName());
    query.bindValue(":PID", getpid());
}

/// \brief Check if the database is ready for use
/// \return true when database is ready, false otherwise
bool DatabaseLogger::isDatabaseReady(void)
{
    bool ready = false;
    MythDB *db = GetMythDB();

    if ((db) && db->HaveValidDatabase())
    {
        if ( !m_loggingTableExists )
            m_loggingTableExists = tableExists(m_handle.string);

        if ( m_loggingTableExists )
            ready = true;
    }

    return ready;
}

/// \brief Checks whether table exists and is ready for writing
/// \param  table  The name of the table to check (without schema name)
/// \return true if table exists in schema or false if not
bool DatabaseLogger::tableExists(const QString &table)
{
    bool result = false;
    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        QString sql = "SELECT INFORMATION_SCHEMA.COLUMNS.COLUMN_NAME "
                      "  FROM INFORMATION_SCHEMA.COLUMNS "
                      " WHERE INFORMATION_SCHEMA.COLUMNS.TABLE_SCHEMA = "
                      "       DATABASE() "
                      "   AND INFORMATION_SCHEMA.COLUMNS.TABLE_NAME = "
                      "       :TABLENAME "
                      "   AND INFORMATION_SCHEMA.COLUMNS.COLUMN_NAME = "
                      "       :COLUMNNAME;";
        if (query.prepare(sql))
        {
            query.bindValue(":TABLENAME", table);
            query.bindValue(":COLUMNNAME", "function");
            if (query.exec() && query.next())
                result = true;
        }
    }
    return result;
}


/// \brief DBLoggerThread constructor
/// \param logger DatabaseLogger instance that this thread belongs to
DBLoggerThread::DBLoggerThread(DatabaseLogger *logger) :
    MThread("DBLogger"),
    m_logger(logger),
    m_queue(new QQueue<LoggingItem *>),
    m_wait(new QWaitCondition()), aborted(false)
{
}

/// \brief DBLoggerThread deconstructor.  Waits for the thread to finish, then
///        Empties what remains in the queue before deleting it.
DBLoggerThread::~DBLoggerThread()
{
    stop();
    wait();

    QMutexLocker qLock(&m_queueMutex);
    while (!m_queue->empty())
        deleteItem(m_queue->dequeue());
    delete m_queue;
    delete m_wait;
    m_queue = NULL;
    m_wait = NULL;
}

/// \brief Start the thread.
void DBLoggerThread::run(void)
{
    RunProlog();

    // Wait a bit before we start logging to the DB..  If we wait too long,
    // then short-running tasks (like mythpreviewgen) will not log to the db
    // at all, and that's undesirable.
    while (true)
    {
        if ((aborted || (gCoreContext && m_logger->isDatabaseReady())))
            break;

        QMutexLocker locker(&m_queueMutex);
        m_wait->wait(locker.mutex(), 100);
    }

    if (!aborted)
    {
        // We want the query to be out of scope before the RunEpilog() so
        // shutdown occurs correctly as otherwise the connection appears still
        // in use, and we get a qWarning on shutdown.
        MSqlQuery *query = new MSqlQuery(MSqlQuery::InitCon());
        m_logger->prepare(*query);

        QMutexLocker qLock(&m_queueMutex);
        while (!aborted || !m_queue->isEmpty())
        {
            if (m_queue->isEmpty())
            {
                m_wait->wait(qLock.mutex(), 100);
                continue;
            }

            LoggingItem *item = m_queue->dequeue();
            if (!item)
                continue;

            qLock.unlock();

            if (item->message[0] != '\0')
            {
                if (!m_logger->logqmsg(*query, item))
                {
                    qLock.relock();
                    m_queue->prepend(item);
                    m_wait->wait(qLock.mutex(), 100);
                    delete query;
                    query = new MSqlQuery(MSqlQuery::InitCon());
                    m_logger->prepare(*query);
                    continue;
                }
            }
            else
            {
                deleteItem(item);
            }

            qLock.relock();
        }

        delete query;

        qLock.unlock();
    }

    RunEpilog();
}

/// \brief Tell the thread to stop by setting the aborted flag.
void DBLoggerThread::stop(void)
{
    QMutexLocker qLock(&m_queueMutex);
    aborted = true;
    m_wait->wakeAll();
}

/// \brief Get the name of the thread that produced the LoggingItem
/// \param item the LoggingItem in question
/// \return C-string of the thread name
char *getThreadName( LoggingItem *item )
{
    static const char  *unknown = "thread_unknown";
    char *threadName;

    if( !item )
        return( (char *)unknown );

    if( !item->threadName )
    {
        QMutexLocker locker(&logThreadMutex);
        if( logThreadHash.contains(item->threadId) )
            threadName = logThreadHash[item->threadId];
        else
            threadName = (char *)unknown;
    }
    else
    {
        threadName = item->threadName;
    }

    return( threadName );
}

/// \brief Get the thread ID of the thread that produced the LoggingItem
/// \param item the LoggingItem in question
/// \return Thread ID of the producing thread, cast to a 64-bit signed integer
/// \notes In different platforms, the actual value returned here will vary.
///        The intention is to get a thread ID that will map well to what is
///        shown in gdb.
int64_t getThreadTid( LoggingItem *item )
{
    pid_t tid = 0;

    if( !item )
        return( 0 );

    QMutexLocker locker(&logThreadTidMutex);
    if( logThreadTidHash.contains(item->threadId) )
        tid = logThreadTidHash[item->threadId];

    return( tid );
}

/// \brief Set the thread ID of the thread that produced the LoggingItem.  This
///        code is actually run in the thread in question as part of the call
///        to LOG()
/// \param item the LoggingItem in question
/// \notes In different platforms, the actual value returned here will vary.
///        The intention is to get a thread ID that will map well to what is
///        shown in gdb.
void setThreadTid( LoggingItem *item )
{
    QMutexLocker locker(&logThreadTidMutex);

    if( ! logThreadTidHash.contains(item->threadId) )
    {
        int64_t tid = 0;

#if defined(linux)
        tid = (int64_t)syscall(SYS_gettid);
#elif defined(__FreeBSD__)
        long lwpid;
        int dummy = thr_self( &lwpid );
        (void)dummy;
        tid = (int64_t)lwpid;
#elif CONFIG_DARWIN
        tid = (int64_t)mach_thread_self();
#endif
        logThreadTidHash[item->threadId] = tid;
    }
}

/// \brief LoggerThread constructor.  Enables debugging of thread registration
///        and deregistration if the VERBOSE_THREADS environment variable is
///        set.
LoggerThread::LoggerThread() :
    MThread("Logger"),
    m_waitNotEmpty(new QWaitCondition()),
    m_waitEmpty(new QWaitCondition()),
    aborted(false)
{
    char *debug = getenv("VERBOSE_THREADS");
    if (debug != NULL)
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "Logging thread registration/deregistration enabled!");
        debugRegistration = true;
    }
}

/// \brief LoggerThread destructor.  Triggers the deletion of all loggers.
LoggerThread::~LoggerThread()
{
    stop();
    wait();

    QMutexLocker locker(&loggerListMutex);

    QList<LoggerBase *>::iterator it;

    for (it = loggerList.begin(); it != loggerList.end(); ++it)
    {
        (*it)->deleteLater();
    }

    delete m_waitNotEmpty;
    delete m_waitEmpty;
}

/// \brief Run the logging thread.  This thread reads from the logging queue,
///        and handles distributing the LoggingItems to each logger instance.
///        The thread will not exit until the logging queue is emptied
///        completely, ensuring that all logging is flushed.
void LoggerThread::run(void)
{
    RunProlog();

    logThreadFinished = false;

    QMutexLocker qLock(&logQueueMutex);

    while (!aborted || !logQueue.isEmpty())
    {
        if (logQueue.isEmpty())
        {
            m_waitEmpty->wakeAll();
            m_waitNotEmpty->wait(qLock.mutex(), 100);
            continue;
        }

        LoggingItem *item = logQueue.dequeue();
        qLock.unlock();

        handleItem(item);
        deleteItem(item);

        qLock.relock();
    }

    logThreadFinished = true;

    qLock.unlock();

    RunEpilog();
}


/// \brief  Handles each LoggingItem, generally by handing it off to the 
///         various running logger instances.  There is a special case for
///         thread registration and deregistration which are also included in
///         the logging queue to keep the thread names in sync with the log
///         messages.
/// \param  item    The LoggingItem to be handled
void LoggerThread::handleItem(LoggingItem *item)
{
    if (item->type & kRegistering)
    {
        int64_t tid = getThreadTid(item);

        QMutexLocker locker(&logThreadMutex);
        logThreadHash[item->threadId] = strdup(item->threadName);

        if (debugRegistration)
        {
            snprintf(item->message, LOGLINE_MAX,
                     "Thread 0x%" PREFIX64 "X (%" PREFIX64 
                     "d) registered as \'%s\'",
                     (long long unsigned int)item->threadId,
                     (long long int)tid,
                     logThreadHash[item->threadId]);
        }
    }
    else if (item->type & kDeregistering)
    {
        int64_t tid = 0;

        {
            QMutexLocker locker(&logThreadTidMutex);
            if( logThreadTidHash.contains(item->threadId) )
            {
                tid = logThreadTidHash[item->threadId];
                logThreadTidHash.remove(item->threadId);
            }
        }

        QMutexLocker locker(&logThreadMutex);
        if (logThreadHash.contains(item->threadId))
        {
            if (debugRegistration)
            {
                snprintf(item->message, LOGLINE_MAX,
                         "Thread 0x%" PREFIX64 "X (%" PREFIX64 
                         "d) deregistered as \'%s\'",
                         (long long unsigned int)item->threadId,
                         (long long int)tid,
                         logThreadHash[item->threadId]);
            }
            item->threadName = logThreadHash[item->threadId];
            logThreadHash.remove(item->threadId);
        }
    }

    if (item->message[0] != '\0')
    {
        QMutexLocker locker(&loggerListMutex);

        QList<LoggerBase *>::iterator it;
        for (it = loggerList.begin(); it != loggerList.end(); ++it)
            (*it)->logmsg(item);
    }
}

/// \brief Stop the thread by setting the abort flag after waiting a second for
///        the queue to be flushed.
void LoggerThread::stop(void)
{
    QMutexLocker qLock(&logQueueMutex);
    flush(1000);
    aborted = true;
    m_waitNotEmpty->wakeAll();
}

/// \brief  Wait for the queue to be flushed (up to a timeout)
/// \param  timeoutMS   The number of ms to wait for the queue to flush
/// \return true if the queue is empty, false otherwise
bool LoggerThread::flush(int timeoutMS)
{
    QTime t;
    t.start();
    while (!aborted && logQueue.isEmpty() && t.elapsed() < timeoutMS)
    {
        m_waitNotEmpty->wakeAll();
        int left = timeoutMS - t.elapsed();
        if (left > 0)
            m_waitEmpty->wait(&logQueueMutex, left);
    }
    return logQueue.isEmpty();
}

static QList<LoggingItem*> item_recycler;
static QAtomicInt item_count;
static QAtomicInt malloc_count;

#define DEBUG_MEMORY 0
#if DEBUG_MEMORY
static int max_count = 0;
static QTime memory_time;
#endif

/// \brief  Create a new LoggingItem
/// \param  _file   filename of the source file where the log message is from
/// \param  _function source function where the log message is from
/// \param  _line   line number in the source where the log message is from
/// \param  _level  logging level of the message (LogLevel_t)
/// \param  _type   type of logging message
/// \return LoggingItem that was created
static LoggingItem *createItem(const char *_file, const char *_function,
                               int _line, LogLevel_t _level, int _type)
{
    LoggingItem *item = new LoggingItem(_file, _function, _line, _level, _type);

    malloc_count.ref();

#if DEBUG_MEMORY
    int val = item_count.fetchAndAddRelaxed(1) + 1;
    if (val == 0)
        memory_time.start();
    max_count = (val > max_count) ? val : max_count;
    if (memory_time.elapsed() > 1000)
    {
        cout<<"current memory usage: "
            <<val<<" * "<<sizeof(LoggingItem)<<endl;
        cout<<"max memory usage: "
            <<max_count<<" * "<<sizeof(LoggingItem)<<endl;
        cout<<"malloc count: "<<(int)malloc_count<<endl;
        memory_time.start();
    }
#else
    item_count.ref();
#endif

    return item;
}

/// \brief  Delete the LoggingItem once its reference count has run down
/// \param  item    LoggingItem to delete.
static void deleteItem(LoggingItem *item)
{
    if (!item)
        return;

    if (!item->refcount.deref())
    {
        if (item->threadName)
            free(item->threadName);
        item_count.deref();
        delete item;
    }
}

/// \brief  Fill in the time structure from the current time to make a timestamp
///         for the log message.  This is run as part of the LOG() call.
/// \param  tm  pointer to the time structure to fill in
/// \param  usec    pointer to a 32bit unsigned int to return the number of us
void LogTimeStamp( struct tm *tm, uint32_t *usec )
{
    if( !usec || !tm )
        return;

    time_t epoch;

#if HAVE_GETTIMEOFDAY
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    epoch = tv.tv_sec;
    *usec  = tv.tv_usec;
#else
    /* Stupid system has no gettimeofday, use less precise QDateTime */
    QDateTime date = QDateTime::currentDateTime();
    QTime     time = date.time();
    epoch = date.toTime_t();
    *usec = time.msec() * 1000;
#endif

    localtime_r(&epoch, tm);
}

/// \brief  Format and send a log message into the queue.  This is called from
///         the LOG() macro.  The intention is minimal blocking of the caller.
/// \param  mask    Verbosity mask of the message (VB_*)
/// \param  level   Log level of this message (LOG_* - matching syslog levels)
/// \param  file    Filename of source code logging the message
/// \param  line    Line number within the source of log message source
/// \param  function    Function name of the log message source
/// \param  fromQString true if this message originated from QString
/// \param  format  printf format string (when not from QString), log message
///                 (when from QString)
/// \param  ...     printf arguments (when not from QString)
void LogPrintLine( uint64_t mask, LogLevel_t level, const char *file, int line,
                   const char *function, int fromQString,
                   const char *format, ... )
{
    va_list         arguments;

    QMutexLocker qLock(&logQueueMutex);

    int type = kMessage;
    type |= (mask & VB_FLUSH) ? kFlush : 0;
    type |= (mask & VB_STDIO) ? kStandardIO : 0;
    LoggingItem *item = createItem(file, function, line, level, type);
    if (!item)
        return;

    char *formatcopy = NULL;
    if( fromQString && strchr(format, '%') )
    {
        QString string(format);
        format = strdup(string.replace(logRegExp, "%%").toLocal8Bit()
                              .constData());
        formatcopy = (char *)format;
    }

    va_start(arguments, format);
    vsnprintf(item->message, LOGLINE_MAX, format, arguments);
    va_end(arguments);

    if (formatcopy)
        free(formatcopy);

    logQueue.enqueue(item);

    if (logThread && logThreadFinished && !logThread->isRunning())
    {
        while (!logQueue.isEmpty())
        {
            item = logQueue.dequeue();
            qLock.unlock();
            logThread->handleItem(item);
            deleteItem(item);
            qLock.relock();
        }
    }
    else if (logThread && !logThreadFinished && (type & kFlush))
    {
        logThread->flush();
    }
}

#ifndef _WIN32
/// \brief  SIGHUP handler - reopen all open logfiles for logrollers
void logSighup( int signum, siginfo_t *info, void *secret )
{
    LOG(VB_GENERAL, LOG_INFO, "SIGHUP received, rolling log files.");

    /* SIGHUP was sent.  Close and reopen debug logfiles */
    QMutexLocker locker(&loggerListMutex);

    QList<LoggerBase *>::iterator it;
    for (it = loggerList.begin(); it != loggerList.end(); ++it)
    {
        (*it)->reopen();
    }
}
#endif

/// \brief Generate the logPropagateArgs global with the latest logging
///        level, mask, etc to propagate to all of the mythtv programs
///        spawned from this one.
void logPropagateCalc(void)
{
    QString mask = verboseString.trimmed();
    mask.replace(QRegExp(" "), ",");
    mask.remove(QRegExp("^,"));
    logPropagateArgs = " --verbose " + mask;

    if (logPropagateOpts.propagate)
        logPropagateArgs += " --logpath " + logPropagateOpts.path;

    QString name = logLevelGetName(logLevel);
    logPropagateArgs += " --loglevel " + name;

    for (int i = 0; i < logPropagateOpts.quiet; i++)
        logPropagateArgs += " --quiet";

    if (!logPropagateOpts.dblog)
        logPropagateArgs += " --nodblog";

#ifndef _WIN32
    if (logPropagateOpts.facility >= 0)
    {
        CODE *syslogname;

        for (syslogname = &facilitynames[0];
             (syslogname->c_name &&
              syslogname->c_val != logPropagateOpts.facility); syslogname++);

        logPropagateArgs += QString(" --syslog %1").arg(syslogname->c_name);
    }
#endif
}

/// \brief Check if we are propagating a "--quiet"
/// \return true if --quiet is being propagated
bool logPropagateQuiet(void)
{
    return logPropagateOpts.quiet;
}

/// \brief  Entry point to start logging for the application.  This will
///         start up all of the threads needed.
/// \param  logfile Filename of the logfile to create.  Empty if no file.
/// \param  progress    non-zero if progress output will be sent to the console.
///                     This squelches all messages less important than LOG_ERR
///                     on the console
/// \param  quiet       quiet level requested (squelches all console output)
/// \param  facility    Syslog facility to use.  -1 to disable syslog output
/// \param  level       Minimum logging level to put into the logs
/// \param  dblog       true if database logging is requested
/// \param  propagate   true if the logfile path needs to be propagated to child
///                     processes.
void logStart(QString logfile, int progress, int quiet, int facility,
              LogLevel_t level, bool dblog, bool propagate)
{
    LoggerBase *logger;

    {
        QMutexLocker qLock(&logQueueMutex);
        if (!logThread)
            logThread = new LoggerThread();
    }

    if (logThread->isRunning())
        return;

    logLevel = level;
    LOG(VB_GENERAL, LOG_NOTICE, QString("Setting Log Level to LOG_%1")
             .arg(logLevelGetName(logLevel).toUpper()));

    logPropagateOpts.propagate = propagate;
    logPropagateOpts.quiet = quiet;
    logPropagateOpts.facility = facility;
    logPropagateOpts.dblog = dblog;

    if (propagate)
    {
        QFileInfo finfo(logfile);
        QString path = finfo.path();
        logPropagateOpts.path = path;
    }

    logPropagateCalc();

    /* log to the console */
    logger = new FileLogger((char *)"-", progress, quiet);

    /* Debug logfile */
    if( !logfile.isEmpty() )
        logger = new FileLogger((char *)logfile.toLocal8Bit().constData(),
                                false, false);

#ifndef _WIN32
    /* Syslog */
    if( facility == -1 )
        LOG(VB_GENERAL, LOG_CRIT,
                 "Syslogging facility unknown, disabling syslog output");
    else if( facility >= 0 )
        logger = new SyslogLogger(facility);
#endif

    /* Database */
    if( dblog )
        logger = new DatabaseLogger((char *)"logging");

#ifndef _WIN32
    /* Setup SIGHUP */
    LOG(VB_GENERAL, LOG_NOTICE, "Setting up SIGHUP handler");
    struct sigaction sa;
    sa.sa_sigaction = logSighup;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction( SIGHUP, &sa, NULL );
#endif

    (void)logger;

    logThread->start();
}

/// \brief  Entry point for stopping logging for an application
void logStop(void)
{
    if (logThread)
    {
        logThread->stop();
        logThread->wait();
    }

#ifndef _WIN32
    /* Tear down SIGHUP */
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = SA_RESTART;
    sigaction( SIGHUP, &sa, NULL );
#endif

    QList<LoggerBase *>::iterator it;
    for (it = loggerList.begin(); it != loggerList.end(); ++it)
    {
        (*it)->stopDatabaseAccess();
    }
}

/// \brief  Register the current thread with the given name.  This is triggered
///         by the RunProlog() call in each thread.
/// \param  name    the name of the thread being registered.  This is used for
///                 indicating the thread each log message is coming from.
void loggingRegisterThread(const QString &name)
{
    if (logThreadFinished)
        return;

    QMutexLocker qLock(&logQueueMutex);

    LoggingItem *item = createItem(__FILE__, __FUNCTION__, __LINE__,
                                   (LogLevel_t)LOG_DEBUG, kRegistering);
    if (item)
    {
        item->threadName = strdup((char *)name.toLocal8Bit().constData());
        logQueue.enqueue(item);
    }
}

/// \brief  Deregister the current thread's name.  This is triggered by the 
///         RunEpilog() call in each thread.
void loggingDeregisterThread(void)
{
    if (logThreadFinished)
        return;

    QMutexLocker qLock(&logQueueMutex);

    LoggingItem *item = createItem(__FILE__, __FUNCTION__, __LINE__,
                                   (LogLevel_t)LOG_DEBUG, kDeregistering);
    if (item)
        logQueue.enqueue(item);
}

/// \brief  Map a syslog facility name back to the enumerated value
/// \param  facility    QString containing the facility name
/// \return Syslog facility as enumerated type.  Negative if not found.
int syslogGetFacility(QString facility)
{
#ifdef _WIN32
    LOG(VB_GENERAL, LOG_NOTICE,
        "Windows does not support syslog, disabling" );
    return( -2 );
#else
    CODE *name;
    int i;
    char *string = (char *)facility.toLocal8Bit().constData();

    for (i = 0, name = &facilitynames[0];
         name->c_name && strcmp(name->c_name, string); i++, name++);

    return( name->c_val );
#endif
}

/// \brief  Map a log level name back to the enumerated value
/// \param  level   QString containing the log level name
/// \return Log level as enumerated type.  LOG_UNKNOWN if not found.
LogLevel_t logLevelGet(QString level)
{
    QMutexLocker locker(&loglevelMapMutex);
    if (!verboseInitialized)
    {
        locker.unlock();
        verboseInit();
        locker.relock();
    }

    for (LoglevelMap::iterator it = loglevelMap.begin();
         it != loglevelMap.end(); ++it)
    {
        LoglevelDef *item = (*it);
        if ( item->name == level.toLower() )
            return (LogLevel_t)item->value;
    }

    return LOG_UNKNOWN;
}

/// \brief  Map a log level enumerated value back to the name
/// \param  level   Enumerated value of the log level
/// \return Log level name.  "unknown" if not found.
QString logLevelGetName(LogLevel_t level)
{
    QMutexLocker locker(&loglevelMapMutex);
    if (!verboseInitialized)
    {
        locker.unlock();
        verboseInit();
        locker.relock();
    }
    LoglevelMap::iterator it = loglevelMap.find((int)level);

    if ( it == loglevelMap.end() )
        return QString("unknown");

    return (*it)->name;
}

/// \brief  Add a verbose level to the verboseMap.  Done at initialization.
/// \param  mask    verbose mask (VB_*)
/// \param  name    name of the verbosity level
/// \param  additive    true if this is to be ORed with other masks.  false if
///                     is will clear the other bits.
/// \param  helptext    Descriptive text for --verbose help output
void verboseAdd(uint64_t mask, QString name, bool additive, QString helptext)
{
    VerboseDef *item = new VerboseDef;

    item->mask = mask;
    name.detach();
    // VB_GENERAL -> general
    name.remove(0, 3);
    name = name.toLower();
    item->name = name;
    item->additive = additive;
    helptext.detach();
    item->helpText = helptext;

    verboseMap.insert(name, item);
}

/// \brief  Add a log level to the logLevelMap.  Done at initialization.
/// \param  value       log level enumerated value (LOG_*) - matches syslog
///                     levels
/// \param  name        name of the log level
/// \param  shortname   one-letter short name for output into logs
void loglevelAdd(int value, QString name, char shortname)
{
    LoglevelDef *item = new LoglevelDef;

    item->value = value;
    name.detach();
    // LOG_CRIT -> crit
    name.remove(0, 4);
    name = name.toLower();
    item->name = name;
    item->shortname = shortname;

    loglevelMap.insert(value, item);
}

/// \brief Initialize the logging levels and verbose levels.
void verboseInit(void)
{
    QMutexLocker locker(&verboseMapMutex);
    QMutexLocker locker2(&loglevelMapMutex);
    verboseMap.clear();
    loglevelMap.clear();

    // This looks funky, so I'll put some explanation here.  The verbosedefs.h
    // file gets included as part of the mythlogging.h include, and at that
    // time, the normal (without _IMPLEMENT_VERBOSE defined) code case will
    // define the VerboseMask enum.  At this point, we force it to allow us
    // to include the file again, but with _IMPLEMENT_VERBOSE set so that the
    // single definition of the VB_* values can be shared to define also the
    // contents of verboseMap, via repeated calls to verboseAdd()

#undef VERBOSEDEFS_H_
#define _IMPLEMENT_VERBOSE
#include "verbosedefs.h"
    
    verboseInitialized = true;
}


/// \brief Outputs the Verbose levels and their descriptions 
///        (for --verbose help)
void verboseHelp(void)
{
    QString m_verbose = verboseString.trimmed();
    m_verbose.replace(QRegExp(" "), ",");
    m_verbose.remove(QRegExp("^,"));

    cerr << "Verbose debug levels.\n"
            "Accepts any combination (separated by comma) of:\n\n";

    for (VerboseMap::Iterator vit = verboseMap.begin();
         vit != verboseMap.end(); ++vit )
    {
        VerboseDef *item = vit.value();
        QString name = QString("  %1").arg(item->name, -15, ' ');
        if (item->helpText.isEmpty())
            continue;
        cerr << name.toLocal8Bit().constData() << " - " << 
                item->helpText.toLocal8Bit().constData() << endl;
    }

    cerr << endl <<
      "The default for this program appears to be: '-v " <<
      m_verbose.toLocal8Bit().constData() << "'\n\n"
      "Most options are additive except for 'none' and 'all'.\n"
      "These two are semi-exclusive and take precedence over any\n"
      "other options.  However, you may use something like\n"
      "'-v none,jobqueue' to receive only JobQueue related messages\n"
      "and override the default verbosity level.\n\n"
      "Additive options may also be subtracted from 'all' by\n"
      "prefixing them with 'no', so you may use '-v all,nodatabase'\n"
      "to view all but database debug messages.\n\n"
      "Some debug levels may not apply to this program.\n\n";
}

/// \brief  Parse the --verbose commandline argument and set the verbose level
/// \param  arg the commandline argument following "--verbose"
/// \return an exit code.  GENERIC_EXIT_OK if all is well.
int verboseArgParse(QString arg)
{
    QString option;

    if (!verboseInitialized)
        verboseInit();

    QMutexLocker locker(&verboseMapMutex);

    verboseMask = verboseDefaultInt;
    verboseString = QString(verboseDefaultStr);

    if (arg.startsWith('-'))
    {
        cerr << "Invalid or missing argument to -v/--verbose option\n";
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    QStringList verboseOpts = arg.split(QRegExp("\\W+"));
    for (QStringList::Iterator it = verboseOpts.begin();
         it != verboseOpts.end(); ++it )
    {
        option = (*it).toLower();
        bool reverseOption = false;

        if (option != "none" && option.left(2) == "no")
        {
            reverseOption = true;
            option = option.right(option.length() - 2);
        }

        if (option == "help")
        {
            verboseHelp();
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        else if (option == "important")
        {
            cerr << "The \"important\" log mask is no longer valid.\n";
        }
        else if (option == "extra")
        {
            cerr << "The \"extra\" log mask is no longer valid.  Please try "
                    "--loglevel debug instead.\n";
        }
        else if (option == "default")
        {
            if (haveUserDefaultValues)
            {
                verboseMask = userDefaultValueInt;
                verboseString = userDefaultValueStr;
            }
            else
            {
                verboseMask = verboseDefaultInt;
                verboseString = QString(verboseDefaultStr);
            }
        }
        else 
        {
            VerboseDef *item = verboseMap.value(option);

            if (item)
            {
                if (reverseOption)
                {
                    verboseMask &= ~(item->mask);
                    verboseString = verboseString.remove(' ' + item->name);
                    verboseString += " no" + item->name;
                }
                else
                {
                    if (item->additive)
                    {
                        if (!(verboseMask & item->mask))
                        {
                            verboseMask |= item->mask;
                            verboseString += ' ' + item->name;
                        }
                    }
                    else
                    {
                        verboseMask = item->mask;
                        verboseString = item->name;
                    }
                }
            }
            else
            {
                cerr << "Unknown argument for -v/--verbose: " << 
                        option.toLocal8Bit().constData() << endl;;
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
    }

    if (!haveUserDefaultValues)
    {
        haveUserDefaultValues = true;
        userDefaultValueInt = verboseMask;
        userDefaultValueStr = verboseString;
    }

    return GENERIC_EXIT_OK;
}

/// \brief Verbose helper function for ENO macro.
/// \param errnum   system errno value
/// \return QString containing the string version of the errno value, plus the
///                 errno value itself.
QString logStrerror(int errnum)
{
    return QString("%1 (%2)").arg(strerror(errnum)).arg(errnum);
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
