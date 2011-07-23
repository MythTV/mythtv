#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QList>
#include <QQueue>
#include <QThread>
#include <QHash>
#include <QCoreApplication>
#include <QFileInfo>
#include <QStringList>
#include <QMap>
#include <QRegExp>
#include <iostream>

using namespace std;

#define _LogLevelNames_
#include "mythlogging.h"
#include "mythconfig.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "dbutil.h"
#include "exitcodes.h"

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
QQueue<LoggingItem_t *> logQueue;
QRegExp                 logRegExp = QRegExp("[%]{1,2}");

QMutex                  logThreadMutex;
QHash<uint64_t, char *> logThreadHash;

QMutex                   logThreadTidMutex;
QHash<uint64_t, int64_t> logThreadTidHash;

LoggerThread            logThread;
bool                    logThreadFinished = false;
bool                    debugRegistration = false;

#ifdef _WIN32
QMutex                  localtimeMutex;
#endif

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
#define MAX_STRING_LENGTH 2048

LogLevel_t logLevel = (LogLevel_t)LOG_INFO;

typedef struct {
    uint64_t    mask;
    QString     name;
    bool        additive;
    QString     helpText;
} VerboseDef;

typedef QMap<QString, VerboseDef *> VerboseMap;

bool verboseInitialized = false;
VerboseMap verboseMap;
QMutex verboseMapMutex;

const uint64_t verboseDefaultInt = VB_GENERAL;
const char    *verboseDefaultStr = " general";

uint64_t verboseMask = verboseDefaultInt;
QString verboseString = QString(verboseDefaultStr);

unsigned int userDefaultValueInt = verboseDefaultInt;
QString      userDefaultValueStr = QString(verboseDefaultStr);
bool         haveUserDefaultValues = false;

void verboseAdd(uint64_t mask, QString name, bool additive, QString helptext);
void verboseInit(void);
void verboseHelp();

void LogTimeStamp( struct tm *tm, uint32_t *usec );
char *getThreadName( LoggingItem_t *item );
int64_t getThreadTid( LoggingItem_t *item );
void setThreadTid( LoggingItem_t *item );
void deleteItem( LoggingItem_t *item );
#ifndef _WIN32
void logSighup( int signum, siginfo_t *info, void *secret );
#endif


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

LoggerBase::~LoggerBase()
{
    QMutexLocker locker(&loggerListMutex);

    QList<LoggerBase *>::iterator it;

    for(it = loggerList.begin(); it != loggerList.end(); it++)
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


FileLogger::FileLogger(char *filename) : LoggerBase(filename, 0),
                                         m_opened(false), m_fd(-1)
{
    if( !strcmp(filename, "-") )
    {
        m_opened = true;
        m_fd = 1;
        LOG(VB_GENERAL, LOG_INFO, "Added logging to the console");
    }
    else
    {
        m_fd = open(filename, O_WRONLY|O_CREAT|O_APPEND, 0664);
        m_opened = (m_fd != -1);
        LOG(VB_GENERAL, LOG_INFO, QString("Added logging to %1")
                 .arg(filename));
    }
}

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

bool FileLogger::logmsg(LoggingItem_t *item)
{
    char                line[MAX_STRING_LENGTH];
    char                usPart[9];
    char                timestamp[TIMESTAMP_MAX];
    int                 length;
    char               *threadName = NULL;
    pid_t               pid = getpid();
    pid_t               tid = 0;

    if (!m_opened)
        return false;

    strftime( timestamp, TIMESTAMP_MAX-8, "%Y-%m-%d %H:%M:%S",
              (const struct tm *)&item->tm );
    snprintf( usPart, 9, ".%06d", (int)(item->usec) );
    strcat( timestamp, usPart );
    length = strlen( timestamp );

    if (m_fd == 1)
    {
        // Stdout
        snprintf( line, MAX_STRING_LENGTH, "%s %c  %s\n", timestamp,
                  LogLevelShortNames[item->level], item->message );
    }
    else
    {
        threadName = getThreadName(item);
        tid = getThreadTid(item);

        if( tid )
            snprintf( line, MAX_STRING_LENGTH, 
                      "%s %c [%d/%d] %s %s:%d (%s) - %s\n",
                      timestamp, LogLevelShortNames[item->level], pid, tid,
                      threadName, item->file, item->line, item->function,
                      item->message );
        else
            snprintf( line, MAX_STRING_LENGTH,
                      "%s %c [%d] %s %s:%d (%s) - %s\n",
                      timestamp, LogLevelShortNames[item->level], pid,
                      threadName, item->file, item->line, item->function,
                      item->message );
    }

    int result = write( m_fd, line, strlen(line) );

    {
        QMutexLocker locker((QMutex *)item->refmutex);
        item->refcount--;
    }

    if( result == -1 )
    {
        LOG(VB_GENERAL, LOG_ERR,
                 QString("Closed Log output on fd %1 due to errors").arg(m_fd));
        m_opened = false;
        if( m_fd != 1 )
            close( m_fd );
        return false;
    }
    deleteItem(item);
    return true;
}


#ifndef _WIN32
SyslogLogger::SyslogLogger(int facility) : LoggerBase(NULL, facility),
                                           m_opened(false)
{
    CODE *name;

    m_application = strdup((char *)QCoreApplication::applicationName()
                           .toLocal8Bit().constData());

    openlog( m_application, LOG_NDELAY | LOG_PID, facility );
    m_opened = true;

    for( name = &facilitynames[0];
         name->c_name && name->c_val != facility; name++ );

    LOG(VB_GENERAL, LOG_INFO, QString("Added syslogging to facility %1")
             .arg(name->c_name));
}

SyslogLogger::~SyslogLogger()
{
    LOG(VB_GENERAL, LOG_INFO, "Removing syslogging");
    free(m_application);
    closelog();
}

bool SyslogLogger::logmsg(LoggingItem_t *item)
{
    if (!m_opened)
        return false;

    syslog( item->level, "%s", item->message );

    {
        QMutexLocker locker((QMutex *)item->refmutex);
        item->refcount--;
    }

    deleteItem(item);
    return true;
}
#endif


DatabaseLogger::DatabaseLogger(char *table) : LoggerBase(table, 0),
                                              m_host(NULL), m_opened(false),
                                              m_loggingTableExists(false)
{
    static const char *queryFmt =
        "INSERT INTO %s (host, application, pid, thread, "
        "msgtime, level, message) VALUES (:HOST, :APPLICATION, "
        ":PID, :THREAD, :MSGTIME, :LEVEL, :MESSAGE)";

    LOG(VB_GENERAL, LOG_INFO, 
             QString("Added database logging to table %1")
             .arg(m_handle.string));

    if (gCoreContext && !gCoreContext->GetHostName().isEmpty())
        m_host = strdup((char *)gCoreContext->GetHostName()
                        .toLocal8Bit().constData());

    m_application = strdup((char *)QCoreApplication::applicationName()
                           .toLocal8Bit().constData());
    m_pid = getpid();

    m_query = (char *)malloc(strlen(queryFmt) + strlen(m_handle.string));
    sprintf(m_query, queryFmt, m_handle.string);

    m_thread = new DBLoggerThread(this);
    m_thread->start();

    m_opened = true;
    m_disabled = false;
}

DatabaseLogger::~DatabaseLogger()
{
    LOG(VB_GENERAL, LOG_INFO, "Removing database logging");

    if( m_thread )
    {
        m_thread->stop();
        m_thread->wait();
        delete m_thread;
    }

    if( m_query )
        free(m_query);
    if( m_application )
        free(m_application);
    if( m_host )
        free(m_host);
}

bool DatabaseLogger::logmsg(LoggingItem_t *item)
{
    if( m_thread )
    {
        if( !m_disabled && m_thread->queueFull() )
        {
            m_disabled = true;
            LOG(VB_GENERAL, LOG_CRIT,
                "Disabling DB Logging: too many messages queued");
            return false;
        }

        if( m_disabled && isDatabaseReady() )
        {
            m_disabled = false;
            LOG(VB_GENERAL, LOG_CRIT, "Reenabling DB Logging");
            usleep(150000);  // Let the queue drain a touch so this won't flap
        }

        if( !m_disabled )
        {
            m_thread->enqueue(item);
            return true;
        }
    }
    return false;
}

bool DatabaseLogger::logqmsg(LoggingItem_t *item)
{
    char        timestamp[TIMESTAMP_MAX];
    char       *threadName = getThreadName(item);

    if( !isDatabaseReady() )
        return false;

    strftime( timestamp, TIMESTAMP_MAX-8, "%Y-%m-%d %H:%M:%S",
              (const struct tm *)&item->tm );

    if( gCoreContext && !m_host )
        m_host = strdup((char *)gCoreContext->GetHostName()
                        .toLocal8Bit().constData());

    MSqlQuery   query(MSqlQuery::LogCon());
    query.prepare( m_query );
    query.bindValue(":HOST",        m_host);
    query.bindValue(":APPLICATION", m_application);
    query.bindValue(":PID",         m_pid);
    query.bindValue(":THREAD",      threadName);
    query.bindValue(":MSGTIME",     timestamp);
    query.bindValue(":LEVEL",       item->level);
    query.bindValue(":MESSAGE",     item->message);

    {
        QMutexLocker locker((QMutex *)item->refmutex);
        item->refcount--;
    }

    if (!query.exec())
    {
        MythDB::DBError("DBLogging", query);
        return false;
    }

    return true;
}

DBLoggerThread::DBLoggerThread(DatabaseLogger *logger) :
    m_logger(logger), m_queue(new QQueue<LoggingItem_t *>),
    m_wait(new QWaitCondition()), aborted(false)
{
}

DBLoggerThread::~DBLoggerThread()
{
    delete m_queue;
    delete m_wait;
}

void DBLoggerThread::run(void)
{
    threadRegister("DBLogger");
    LoggingItem_t *item;

    QMutexLocker qLock(&m_queueMutex);

    while(!aborted || !m_queue->isEmpty())
    {
        if (m_queue->isEmpty())
        {
            m_wait->wait(qLock.mutex(), 100);
            continue;
        }

        item = m_queue->dequeue();
        if (!item)
            continue;

        qLock.unlock();

        if( item->message && !aborted && !m_logger->logqmsg(item) )
        {
            qLock.relock();
            m_queue->prepend(item);
            m_wait->wait(qLock.mutex(), 100);
        } else {
            deleteItem(item);
            qLock.relock();
        }
    }

    MSqlQuery::CloseLogCon();
    threadDeregister();
}

void DBLoggerThread::stop(void)
{
    QMutexLocker qLock(&m_queueMutex);
    aborted = true;
    m_wait->wakeAll();
}

bool DatabaseLogger::isDatabaseReady()
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

/**
 *  \brief Checks whether table exists
 *
 *  \param  table  The name of the table to check (without schema name)
 *  \return true if table exists in schema or false if not
 */
bool DatabaseLogger::tableExists(const QString &table)
{
    bool result = false;
    MSqlQuery query(MSqlQuery::LogCon());
    if (query.isConnected())
    {
        QString sql = "SELECT INFORMATION_SCHEMA.TABLES.TABLE_NAME "
                      "  FROM INFORMATION_SCHEMA.TABLES "
                      " WHERE INFORMATION_SCHEMA.TABLES.TABLE_SCHEMA = "
                      "       DATABASE() "
                      "   AND INFORMATION_SCHEMA.TABLES.TABLE_NAME = "
                      "       :TABLENAME ;";
        if (query.prepare(sql))
        {
            query.bindValue(":TABLENAME", table);
            if (query.exec() && query.next())
                result = true;
        }
    }
    return result;
}



char *getThreadName( LoggingItem_t *item )
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

int64_t getThreadTid( LoggingItem_t *item )
{
    pid_t tid = 0;

    if( !item )
        return( 0 );

    QMutexLocker locker(&logThreadTidMutex);
    if( logThreadTidHash.contains(item->threadId) )
        tid = logThreadTidHash[item->threadId];

    return( tid );
}

void setThreadTid( LoggingItem_t *item )
{
    int64_t tid = 0;

    QMutexLocker locker(&logThreadTidMutex);

    if( ! logThreadTidHash.contains(item->threadId) )
    {
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


LoggerThread::LoggerThread() :
    m_wait(new QWaitCondition()), aborted(false)
{
    char *debug = getenv("VERBOSE_THREADS");
    if (debug != NULL)
    {
        LOG(VB_GENERAL, LOG_CRIT, "Logging thread registration/deregistration "
                                  "enabled!");
        debugRegistration = true;
    }
}

LoggerThread::~LoggerThread()
{
    QMutexLocker locker(&loggerListMutex);

    QList<LoggerBase *>::iterator it;

    for(it = loggerList.begin(); it != loggerList.end(); it++)
    {
        (*it)->deleteLater();
    }

    delete m_wait;
}

void LoggerThread::run(void)
{
    threadRegister("Logger");
    LoggingItem_t *item;

    logThreadFinished = false;

    QMutexLocker qLock(&logQueueMutex);

    while(!aborted || !logQueue.isEmpty())
    {
        if (logQueue.isEmpty())
        {
            m_wait->wait(qLock.mutex(), 100);
            continue;
        }

        item = logQueue.dequeue();
        if (!item)
            continue;

        qLock.unlock();

        if (item->registering)
        {
            int64_t tid = getThreadTid(item);

            QMutexLocker locker(&logThreadMutex);
            logThreadHash[item->threadId] = strdup(item->threadName);

            if( debugRegistration )
            {
                item->message   = (char *)malloc(LOGLINE_MAX+1);
                if( item->message )
                {
                    snprintf( item->message, LOGLINE_MAX,
                              "Thread 0x%llX (%lld) registered as \'%s\'",
                              (long long unsigned int)item->threadId,
                              (long long int)tid, 
                              logThreadHash[item->threadId] );
                }
            }
        }
        else if (item->deregistering)
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
            if( logThreadHash.contains(item->threadId) )
            {
                if( debugRegistration )
                {
                    item->message   = (char *)malloc(LOGLINE_MAX+1);
                    if( item->message )
                    {
                        snprintf( item->message, LOGLINE_MAX,
                                  "Thread 0x%llX (%lld) deregistered as \'%s\'",
                                  (long long unsigned int)item->threadId,
                                  (long long int)tid, 
                                  logThreadHash[item->threadId] );
                    }
                }
                item->threadName = logThreadHash[item->threadId];
                logThreadHash.remove(item->threadId);
            }
        }

        if( item->message )
        {
            QMutexLocker locker(&loggerListMutex);

            QList<LoggerBase *>::iterator it;

            item->refcount = loggerList.size();
            item->refmutex = new QMutex;

            for(it = loggerList.begin(); it != loggerList.end(); it++)
            {
                if( !(*it)->logmsg(item) )
                    deleteItem(item);
            }
        }

        qLock.relock();
    }

    logThreadFinished = true;
}

void LoggerThread::stop(void)
{
    QMutexLocker qLock(&logQueueMutex);
    aborted = true;
    m_wait->wakeAll();
}

void deleteItem( LoggingItem_t *item )
{
    if( !item )
        return;

    {
        QMutexLocker locker((QMutex *)item->refmutex);
        if( item->refcount != 0 )
            return;

        if( item->message )
            free(item->message);

        if( item->threadName )
            free( item->threadName );
    }

    delete (QMutex *)item->refmutex;
    delete item;
}

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

#ifndef _WIN32
    localtime_r(&epoch, tm);
#else
    {
        QMutexLocker timeLock(&localtimeMutex);
        struct tm *tmp = localtime(&epoch);
        memcpy(tm, tmp, sizeof(struct tm));
    }
#endif
}

void LogPrintLine( uint64_t mask, LogLevel_t level, const char *file, int line,
                   const char *function, int fromQString,
                   const char *format, ... )
{
    va_list         arguments;
    char           *message;
    LoggingItem_t  *item;

    // Discard any LOG_ANY attempts
    if( level < 0 )
        return;

    if( !VERBOSE_LEVEL_CHECK(mask, level) )
        return;

    item = new LoggingItem_t;
    if (!item)
        return;

    memset( item, 0, sizeof(LoggingItem_t) );

    message = (char *)malloc(LOGLINE_MAX+1);
    if( !message )
        return;

    QMutexLocker qLock(&logQueueMutex);

    if( fromQString && strchr(format, '%') )
    {
        QString string(format);
        format = string.replace(logRegExp, "%%").toLocal8Bit().constData();
    }

    va_start(arguments, format);
    vsnprintf(message, LOGLINE_MAX, format, arguments);
    va_end(arguments);

    LogTimeStamp( &item->tm, &item->usec );
    item->level    = level;
    item->file     = file;
    item->line     = line;
    item->function = function;
    item->threadId = (uint64_t)QThread::currentThreadId();
    item->message  = message;
    setThreadTid(item);

    logQueue.enqueue(item);
}

#ifndef _WIN32
void logSighup( int signum, siginfo_t *info, void *secret )
{
    LOG(VB_GENERAL, LOG_INFO, "SIGHUP received, rolling log files.");

    /* SIGHUP was sent.  Close and reopen debug logfiles */
    QMutexLocker locker(&loggerListMutex);

    QList<LoggerBase *>::iterator it;
    for(it = loggerList.begin(); it != loggerList.end(); it++)
    {
        (*it)->reopen();
    }
}
#endif

void logPropagateCalc(void)
{
    QString mask = verboseString.trimmed();
    mask.replace(QRegExp(" "), ",");
    mask.remove(QRegExp("^,"));
    logPropagateArgs = " --verbose " + mask;

    if (logPropagateOpts.propagate)
        logPropagateArgs += " --logpath " + logPropagateOpts.path;

    QString name = QString(LogLevelNames[logLevel]).toLower();
    name.remove(0, 4);
    logPropagateArgs += " --loglevel " + name;

    if (logPropagateOpts.quiet)
        logPropagateArgs += " --quiet";

    if (!logPropagateOpts.dblog)
        logPropagateArgs += " --nodblog";

#ifndef _WIN32
    if (logPropagateOpts.facility >= 0)
    {
        CODE *syslogname;

        for( syslogname = &facilitynames[0];
             (syslogname->c_name &&
              syslogname->c_val != logPropagateOpts.facility); syslogname++ );

        logPropagateArgs += QString(" --syslog %1").arg(syslogname->c_name);
    }
#endif
}

bool logPropagateQuiet(void)
{
    return logPropagateOpts.quiet;
}

void logStart(QString logfile, int quiet, int facility, LogLevel_t level,
              bool dblog, bool propagate)
{
    LoggerBase *logger;

    if (logThread.isRunning())
        return;
 
    logLevel = level;
    LOG(VB_GENERAL, LOG_CRIT, QString("Setting Log Level to %1")
             .arg(LogLevelNames[logLevel]));

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
    if( !quiet )
        logger = new FileLogger((char *)"-");

    /* Debug logfile */
    if( !logfile.isEmpty() )
        logger = new FileLogger((char *)logfile.toLocal8Bit().constData());

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
    LOG(VB_GENERAL, LOG_CRIT, "Setting up SIGHUP handler");
    struct sigaction sa;
    sa.sa_sigaction = logSighup;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction( SIGHUP, &sa, NULL );
#endif

    logThread.start();
}

void logStop(void)
{
    logThread.stop();
    logThread.wait();

#ifndef _WIN32
    /* Tear down SIGHUP */
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = SA_RESTART;
    sigaction( SIGHUP, &sa, NULL );
#endif

    QMutexLocker locker(&loggerListMutex);
    QList<LoggerBase *>::iterator it;

    for(it = loggerList.begin(); it != loggerList.end(); )
    {
        locker.unlock();
        delete *it;
        locker.relock();
        it = loggerList.begin();
    }
}

void threadRegister(QString name)
{
    uint64_t id = (uint64_t)QThread::currentThreadId();

    LoggingItem_t *item = new LoggingItem_t;
    if (!item)
        return;

    if (logThreadFinished)
        return;

    memset( item, 0, sizeof(LoggingItem_t) );
    LogTimeStamp( &item->tm, &item->usec );
    item->level = (LogLevel_t)LOG_DEBUG;
    item->threadId = id;
    item->line = __LINE__;
    item->file = (char *)__FILE__;
    item->function = (char *)__FUNCTION__;
    item->threadName = strdup((char *)name.toLocal8Bit().constData());
    item->registering = true;
    setThreadTid(item);

    QMutexLocker qLock(&logQueueMutex);
    logQueue.enqueue(item);
}

void threadDeregister(void)
{
    uint64_t id = (uint64_t)QThread::currentThreadId();
    LoggingItem_t  *item;

    item = new LoggingItem_t;
    if (!item)
        return;

    if (logThreadFinished)
        return;

    memset( item, 0, sizeof(LoggingItem_t) );
    LogTimeStamp( &item->tm, &item->usec );
    item->level = (LogLevel_t)LOG_DEBUG;
    item->threadId = id;
    item->line = __LINE__;
    item->file = (char *)__FILE__;
    item->function = (char *)__FUNCTION__;
    item->deregistering = true;

    QMutexLocker qLock(&logQueueMutex);
    logQueue.enqueue(item);
}

int syslogGetFacility(QString facility)
{
#ifdef _WIN32
    LOG(VB_GENERAL, LOG_CRIT, "Windows does not support syslog,"
                                   " disabling" );
    return( -2 );
#else
    CODE *name;
    int i;
    char *string = (char *)facility.toLocal8Bit().constData();

    for( i = 0, name = &facilitynames[0];
         name->c_name && strcmp(name->c_name, string); i++, name++ );

    return( name->c_val );
#endif
}

LogLevel_t logLevelGet(QString level)
{
    int i;

    level = "LOG_" + level.toUpper();
    for( i = LOG_EMERG; i < LOG_UNKNOWN; i++ )
    {
        if( level == LogLevelNames[i] )
            return (LogLevel_t)i;
    }

    return LOG_UNKNOWN;
}


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

void verboseInit(void)
{
    QMutexLocker locker(&verboseMapMutex);
    verboseMap.clear();

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

void verboseHelp()
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
        cerr << name.toLocal8Bit().constData() << " - " << 
                item->helpText.toLocal8Bit().constData() << endl;
    }

    cerr << endl <<
      "The default for this program appears to be: '-v " <<
      m_verbose.toLocal8Bit().constData() << "'\n\n"
      "Most options are additive except for none, and all.\n"
      "These two are semi-exclusive and take precedence over any\n"
      "prior options given.  You can however use something like\n"
      "'-v none,jobqueue' to get only JobQueue related messages\n"
      "and override the default verbosity level.\n\n"
      "The additive options may also be subtracted from 'all' by \n"
      "prefixing them with 'no', so you may use '-v all,nodatabase'\n"
      "to view all but database debug messages.\n\n"
      "Some debug levels may not apply to this program.\n\n";
}

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
            cerr << "The \"extra\" log mask is no longer valid.  Please try --loglevel debug instead.\n";
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
                    verboseString += " no" + item->name;
                }
                else
                {
                    if (item->additive)
                    {
                        verboseMask |= item->mask;
                        verboseString += ' ' + item->name;
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

// Verbose helper function for ENO macro
QString logStrerror(int errnum)
{
    return QString("%1 (%2)").arg(strerror(errnum)).arg(errnum);
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
