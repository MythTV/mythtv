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
QQueue<LoggingItem_t *> logQueue;

QMutex                  logThreadMutex;
QHash<uint64_t, char *> logThreadHash;

LoggerThread            logThread;

#define TIMESTAMP_MAX 30
#define MAX_STRING_LENGTH 2048

LogLevel_t LogLevel = LOG_UNKNOWN;  /**< The log level mask to apply, messages
                                         must be at at least this priority to
                                         be output */

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
    static const char  *unknown = "thread_unknown";
    char               *threadName = NULL;
    pid_t               pid = getpid();

    if (!m_opened)
        return false;

    strftime( timestamp, TIMESTAMP_MAX-8, "%Y-%b-%d %H:%M:%S",
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

        snprintf( line, MAX_STRING_LENGTH, "%s [%d] %s %s:%d (%s) - %s\n", 
                  timestamp, pid, threadName, item->file, item->line, 
                  item->function, item->message );
    }

    int result = write( m_fd, line, strlen(line) );
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
    return true;
}


DatabaseLogger::DatabaseLogger(char *table) : LoggerBase(table, 0), 
                                              m_opened(false)
{
}

DatabaseLogger::~DatabaseLogger()
{
}

bool DatabaseLogger::logmsg(LoggingItem_t *item)
{
    return false;
}


void LoggerThread::run(void)
{
    threadRegister("Logger");
    LoggingItem_t *item;

    aborted = false;

    while(!aborted)
    {
        if (logQueue.empty())
        {
            msleep(100);
            continue;
        }

        item = logQueue.dequeue();
        if (!item)
            continue;

        if (item->registering)
        {
            QMutexLocker locker(&logThreadMutex);
            logThreadHash[item->threadId] = strdup(item->threadName);
            item->message   = (char *)malloc(LOGLINE_MAX+1);
            if( item->message )
            {
                snprintf( item->message, LOGLINE_MAX,
                          "Thread 0x%llX registered as \'%s\'", 
                          (long long unsigned int)item->threadId, 
                          logThreadHash[item->threadId] );
            }
        }
        else if (item->deregistering)
        {
            QMutexLocker locker(&logThreadMutex);
            if( logThreadHash.contains(item->threadId) )
            {
                item->message   = (char *)malloc(LOGLINE_MAX+1);
                if( item->message )
                {
                    snprintf( item->message, LOGLINE_MAX,
                              "Thread 0x%llX deregistered as \'%s\'", 
                              (long long unsigned int)item->threadId, 
                              logThreadHash[item->threadId] );
                }
                item->threadName = logThreadHash[item->threadId];
                logThreadHash.remove(item->threadId);
            }
        }

        if( item->message )
        {
            QMutexLocker locker(&loggerListMutex);

            QList<LoggerBase *>::iterator it;

            for(it = loggerList.begin(); it != loggerList.end(); it++)
            {
                (*it)->logmsg(item);
            }

            free(item->message);
        }

        if( item->threadName )
            free( item->threadName );

        delete item;
    }
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

    logQueue.enqueue(item);
}

void logStart(QString logfile)
{
    LoggerBase *logger;

    if (!logThread.isRunning())
    {
        /* log to the console */
        logger = new FileLogger((char *)"-");
        if( !logfile.isEmpty() )
            logger = new FileLogger((char *)logfile.toLocal8Bit().constData());
        logger = new SyslogLogger(LOG_LOCAL7);

        logThread.start();
    }
}

void logStop(void)
{
    logThread.stop();
    logThread.wait();
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
    logQueue.enqueue(item);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
