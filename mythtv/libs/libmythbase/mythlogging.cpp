#include <QMutex>
#include <QMutexLocker>
#include <QList>
#include <QQueue>
#include <QThread>
#include <QHash>
#include <QCoreApplication>

#define _LogLevelNames_
#include "mythlogging.h"
#include "mythverbose.h"
#include "mythconfig.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "dbutil.h"

#include <stdlib.h>
#define SYSLOG_NAMES
#include <syslog.h>
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

QMutex                  loggerListMutex;
QList<LoggerBase *>     loggerList;

QMutex                  logQueueMutex;
QQueue<LoggingItem_t *> logQueue;

QMutex                  logThreadMutex;
QHash<uint64_t, char *> logThreadHash;

LoggerThread            logThread;
bool                    debugRegistration = false;

#define TIMESTAMP_MAX 30
#define MAX_STRING_LENGTH 2048

LogLevel_t LogLevel = LOG_UNKNOWN;  /**< The log level mask to apply, messages
                                         must be at at least this priority to
                                         be output */

char *getThreadName( LoggingItem_t *item );
void deleteItem( LoggingItem_t *item );

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
        LogPrintNoArg( VB_IMPORTANT, LOG_INFO, "Added logging to the console" );
    }
    else
    {
        m_fd = open(filename, O_WRONLY|O_CREAT|O_APPEND|O_SYNC, 0664);
        m_opened = (m_fd != -1);
        LogPrint( VB_IMPORTANT, LOG_INFO, "Added logging to %s", filename );
    }
}

FileLogger::~FileLogger()
{
    if( m_opened ) 
    {
        if( m_fd != 1 )
        {
            LogPrint( VB_IMPORTANT, LOG_INFO, "Removed logging to %s", 
                      m_handle.string );
            close( m_fd );
        }
        else
            LogPrintNoArg( VB_IMPORTANT, LOG_INFO,
                           "Removed logging to the console" );
    }
}

bool FileLogger::logmsg(LoggingItem_t *item)
{
    char                line[MAX_STRING_LENGTH];
    char                usPart[9];
    char                timestamp[TIMESTAMP_MAX];
    int                 length;
    char               *threadName = NULL;
    pid_t               pid = getpid();

    if (!m_opened)
        return false;

    strftime( timestamp, TIMESTAMP_MAX-8, "%Y-%m-%d %H:%M:%S",
              (const struct tm *)&item->tm );
    snprintf( usPart, 9, ".%06d ", (int)(item->usec) );
    strcat( timestamp, usPart );
    length = strlen( timestamp );

    if (m_fd == 1) 
    {
        // Stdout
        snprintf( line, MAX_STRING_LENGTH, "%s %s\n", timestamp, 
                  item->message );
    }
    else 
    {
        threadName = getThreadName(item);

        snprintf( line, MAX_STRING_LENGTH, "%s [%d] %s %s:%d (%s) - %s\n", 
                  timestamp, pid, threadName, item->file, item->line, 
                  item->function, item->message );
    }

    int result = write( m_fd, line, strlen(line) );

    {
        QMutexLocker locker((QMutex *)item->refmutex);
        item->refcount--;
    }

    if( result == -1 )
    {  
        LogPrint( VB_IMPORTANT, LOG_UNKNOWN, 
                  "Closed Log output on fd %d due to errors", m_fd );
        m_opened = false;
        if( m_fd != 1 )
            close( m_fd );
        return false;
    }
    return true;
}


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

    LogPrint(VB_IMPORTANT, LOG_INFO, "Added syslogging to facility %s",
             name->c_name);
}

SyslogLogger::~SyslogLogger()
{
    LogPrintNoArg(VB_IMPORTANT, LOG_INFO, "Removing syslogging");
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

    return true;
}


DatabaseLogger::DatabaseLogger(char *table) : LoggerBase(table, 0), 
                                              m_host(NULL), m_opened(false),
                                              m_loggingTableExists(false)
{
    static const char *queryFmt =
        "INSERT INTO %s (host, application, pid, thread, "
        "msgtime, level, message) VALUES (:HOST, :APPLICATION, "
        ":PID, :THREAD, :MSGTIME, :LEVEL, :MESSAGE)";

    LogPrint(VB_IMPORTANT, LOG_INFO, "Added database logging to table %s",
             m_handle.string);

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
}

DatabaseLogger::~DatabaseLogger()
{
    LogPrintNoArg(VB_IMPORTANT, LOG_INFO, "Removing database logging");

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
        m_thread->enqueue(item);
    return true;
}

bool DatabaseLogger::logqmsg(LoggingItem_t *item)
{
    char        timestamp[TIMESTAMP_MAX];
    char       *threadName = getThreadName(item);;

    if( !isDatabaseReady() )
        return false;

    strftime( timestamp, TIMESTAMP_MAX-8, "%Y-%m-%d %H:%M:%S",
              (const struct tm *)&item->tm );

    if( gCoreContext && !m_host )
        m_host = strdup((char *)gCoreContext->GetHostName()
                        .toLocal8Bit().constData());

    MSqlQuery   query(MSqlQuery::InitCon());
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

void DBLoggerThread::run(void)
{
    threadRegister("DBLogger");
    LoggingItem_t *item;

    aborted = false;

    while(!aborted || !m_queue->isEmpty())
    {
        if (m_queue->isEmpty())
        {
            msleep(100);
            continue;
        }

        item = m_queue->dequeue();
        if (!item)
            continue;

        if( item->message && !aborted )
        {
            m_logger->logqmsg(item);
        }

        deleteItem(item);
    }
}

bool DatabaseLogger::isDatabaseReady()
{
    bool ready = false;
    MythDB *db;

    if ( !m_loggingTableExists )
        m_loggingTableExists = DBUtil::TableExists(m_handle.string);

    if ( m_loggingTableExists && (db = GetMythDB()) && db->HaveValidDatabase() )
        ready = true;

    return ready;
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

LoggerThread::LoggerThread()
{
    char *debug = getenv("VERBOSE_THREADS");
    if (debug != NULL)
    {
        VERBOSE(VB_IMPORTANT, "Logging thread registration/deregistration "
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
}
 
void LoggerThread::run(void)
{
    threadRegister("Logger");
    LoggingItem_t *item;

    aborted = false;

    QMutexLocker qLock(&logQueueMutex);

    while(!aborted || !logQueue.isEmpty())
    {
        if (logQueue.isEmpty())
        {
            qLock.unlock();
            msleep(100);
            qLock.relock();
            continue;
        }

        item = logQueue.dequeue();
        if (!item)
            continue;

        qLock.unlock();

        if (item->registering)
        {
            QMutexLocker locker(&logThreadMutex);
            logThreadHash[item->threadId] = strdup(item->threadName);
            if( debugRegistration )
            {
                item->message   = (char *)malloc(LOGLINE_MAX+1);
                if( item->message )
                {
                    snprintf( item->message, LOGLINE_MAX,
                              "Thread 0x%llX registered as \'%s\'", 
                              (long long unsigned int)item->threadId, 
                              logThreadHash[item->threadId] );
                }
            }
        }
        else if (item->deregistering)
        {
            QMutexLocker locker(&logThreadMutex);
            if( logThreadHash.contains(item->threadId) )
            {
                if( debugRegistration )
                {
                    item->message   = (char *)malloc(LOGLINE_MAX+1);
                    if( item->message )
                    {
                        snprintf( item->message, LOGLINE_MAX,
                                  "Thread 0x%llX deregistered as \'%s\'", 
                                  (long long unsigned int)item->threadId, 
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
                (*it)->logmsg(item);
            }
        }

        deleteItem(item);

        qLock.relock();
    }
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

void LogTimeStamp( time_t *epoch, uint32_t *usec )
{
    if( !epoch || !usec )
        return;

#if HAVE_GETTIMEOFDAY
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    *epoch = tv.tv_sec; 
    *usec  = tv.tv_usec;
#else
    /* Stupid system has no gettimeofday, use less precise QDateTime */
    QDateTime date = QDateTime::currentDateTime();
    QTime     time = date.time();
    *epoch = date.toTime_t();
    *usec = time.msec() * 1000;
#endif
}

void LogPrintLine( uint32_t mask, LogLevel_t level, char *file, int line, 
                   char *function, char *format, ... )
{
    va_list         arguments;
    char           *message;

    message = (char *)malloc(LOGLINE_MAX+1);
    if( !message )
        return;

    va_start(arguments, format);
    vsnprintf(message, LOGLINE_MAX, format, arguments);
    va_end(arguments);

    LogPrintLineNoArg( mask, level, file, line, function, message );
    free( message );
}

void LogPrintLineNoArg( uint32_t mask, LogLevel_t level, char *file, int line, 
                        char *function, char *message )
{
    LoggingItem_t  *item;
    time_t          epoch;
    uint32_t        usec;

    if( !VERBOSE_LEVEL_CHECK(mask) )
        return;

    if( level > LogLevel )
        return;

    item = new LoggingItem_t;
    if (!item)
        return;

    memset( item, 0, sizeof(LoggingItem_t) );

    LogTimeStamp( &epoch, &usec );

    localtime_r(&epoch, &item->tm);
    item->usec = usec;

    item->level    = level;
    item->file     = file;
    item->line     = line;
    item->function = function; 
    item->threadId = (uint64_t)QThread::currentThreadId();

    item->message   = strdup(message);
    if( !item->message ) 
    {
        delete item;
        return;
    }

    QMutexLocker qLock(&logQueueMutex);
    logQueue.enqueue(item);
}

void logStart(QString logfile, int quiet, int facility, bool dblog)
{
    LoggerBase *logger;

    if (!logThread.isRunning())
    {
        /* log to the console */
        if( !quiet )
            logger = new FileLogger((char *)"-");

        /* Debug logfile */
        if( !logfile.isEmpty() )
            logger = new FileLogger((char *)logfile.toLocal8Bit().constData());

        /* Syslog */
        if( facility < 0 )
            LogPrintNoArg(VB_IMPORTANT, LOG_CRIT, 
                          "Syslogging facility unknown, disabling syslog "
                          "output");
        else if( facility > 0 )
            logger = new SyslogLogger(facility);

        /* Database */
        if( dblog )
            logger = new DatabaseLogger((char *)"logging");

        logThread.start();
    }
}

void logStop(void)
{
    logThread.stop();
    logThread.wait();

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
    LoggingItem_t  *item;
    time_t epoch;
    uint32_t usec;

    item = new LoggingItem_t;
    if (!item)
        return;

    memset( item, 0, sizeof(LoggingItem_t) );
    LogTimeStamp( &epoch, &usec );

    localtime_r(&epoch, &item->tm);
    item->usec = usec;

    item->level = (LogLevel_t)LOG_DEBUG;
    item->threadId = id;
    item->line = __LINE__;
    item->file = (char *)__FILE__;
    item->function = (char *)__FUNCTION__;
    item->threadName = strdup((char *)name.toLocal8Bit().constData());
    item->registering = true;

    QMutexLocker qLock(&logQueueMutex);
    logQueue.enqueue(item);
}

void threadDeregister(void)
{
    uint64_t id = (uint64_t)QThread::currentThreadId();
    LoggingItem_t  *item;
    time_t epoch;
    uint32_t usec;

    item = new LoggingItem_t;
    if (!item)
        return;

    memset( item, 0, sizeof(LoggingItem_t) );
    LogTimeStamp( &epoch, &usec );

    localtime_r(&epoch, &item->tm);
    item->usec = usec;

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
    CODE *name;
    int i;
    char *string = (char *)facility.toLocal8Bit().constData();
    
    for( i = 0, name = &facilitynames[0]; 
         name->c_name && strcmp(name->c_name, string); i++, name++ );

    return( name->c_val );
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
