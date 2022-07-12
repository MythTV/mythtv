#include <fstream>

#include <QtGlobal>
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
#include <QSocketNotifier>
#include <iostream>

#include "mythlogging.h"
#include "logging.h"
#include "loggingserver.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "dbutil.h"
#include "exitcodes.h"
#include "compat.h"

#include <cstdlib>
#ifndef _WIN32
#include "mythsyslog.h"
#if CONFIG_SYSTEMD_JOURNAL
#define SD_JOURNAL_SUPPRESS_LOCATION 1 // NOLINT(cppcoreguidelines-macro-usage)
#include <systemd/sd-journal.h>
#endif
#endif
#include <cstdarg>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cstdio>
#include <unistd.h>
#if HAVE_GETTIMEOFDAY
#include <sys/time.h>
#endif
#include <csignal>

// Various ways to get to thread's tid
#if defined(__linux__)
#include <sys/syscall.h>
#elif defined(__FreeBSD__)
extern "C" {
#include <sys/ucontext.h>
#include <sys/thr.h>
}
#elif defined(Q_OS_DARWIN)
#include <mach/mach.h>
#endif

static QMutex                      loggerMapMutex;
static QMap<QString, LoggerBase *> loggerMap;

LogForwardThread                   *logForwardThread = nullptr;

using LoggerList = QList<LoggerBase *>;

struct LoggerListItem {
    LoggerList *m_itemList;
    std::chrono::seconds m_itemEpoch;
};

// A list of logging objects that process messages.
static QMutex                       gLoggerListMutex;
static LoggerListItem               *gLoggerList {nullptr};

// This is a FIFO queue containing the incoming messages "sent" from a
// client to this server. As each message arrives, it is queued here
// for later retrieval by a different thread.  This used to be
// populated by a thread that received messages from the network, and
// drained by a different thread that logged the messages.  It is now
// populated by the thread that generated the message, and drained by
// a different thread that logs the messages.
static QMutex                       gLogItemListMutex;
static LoggingItemList              gLogItemList;
static QWaitCondition               gLogItemListNotEmpty;

/// \brief LoggerBase class constructor.  Adds the new logger instance to the
///        loggerMap.
/// \param string a C-string of the handle for this instance (NULL if unused)
LoggerBase::LoggerBase(const char *string) :
    m_handle(string)
{
    QMutexLocker locker(&loggerMapMutex);
    loggerMap.insert(m_handle, this);
}


/// \brief LoggerBase deconstructor.  Removes the logger instance from the
///        loggerMap.
LoggerBase::~LoggerBase()
{
    QMutexLocker locker(&loggerMapMutex);
    loggerMap.remove(m_handle);
}


/// \brief FileLogger constructor
/// \param filename Filename of the logfile.
FileLogger::FileLogger(const char *filename) :
        LoggerBase(filename),
        m_ofstream(filename, std::ios::app)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Added logging to %1")
             .arg(filename));
}


/// \brief FileLogger deconstructor - close the logfile
FileLogger::~FileLogger()
{
    if(m_ofstream.is_open())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Removed logging to %1")
            .arg(m_handle));
        m_ofstream.close();
    }
}

FileLogger *FileLogger::create(const QString& filename, QMutex *mutex)
{
    QByteArray ba = filename.toLocal8Bit();
    const char *file = ba.constData();
    auto *logger =
        dynamic_cast<FileLogger *>(loggerMap.value(filename, nullptr));

    if (logger)
        return logger;

    // Need to add a new FileLogger
    mutex->unlock();
    // inserts into loggerMap
    logger = new FileLogger(file);
    mutex->lock();

    return logger;
}

/// \brief Reopen the logfile after a SIGHUP.  Log files only (no console).
///        This allows for logrollers to be used.
void FileLogger::reopen(void)
{
    m_ofstream.close();

    m_ofstream.open(qPrintable(m_handle), std::ios::app);
    LOG(VB_GENERAL, LOG_INFO, QString("Rolled logging on %1") .arg(m_handle));
}

/// \brief Process a log message, writing to the logfile
/// \param item LoggingItem containing the log message to process
bool FileLogger::logmsg(LoggingItem *item)
{
    if (!m_ofstream.is_open())
        return false;

    std::string line = item->toString();

    m_ofstream << line;

    if (m_ofstream.bad())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Closed Log output to %1 due to unrecoverable error(s).").arg(m_handle));
        m_ofstream.close();
        return false;
    }
    return true;
}

#ifndef _WIN32
/// \brief SyslogLogger constructor \param facility Syslog facility to
/// use in logging
SyslogLogger::SyslogLogger(bool open) :
    LoggerBase(nullptr)
{
    if (open)
    {
        openlog(nullptr, LOG_NDELAY, 0 );
        m_opened = true;
    }

    LOG(VB_GENERAL, LOG_INFO, "Added syslogging");
}

/// \brief SyslogLogger deconstructor.
SyslogLogger::~SyslogLogger()
{
    LOG(VB_GENERAL, LOG_INFO, "Removing syslogging");
    if (m_opened)
        closelog();
}

SyslogLogger *SyslogLogger::create(QMutex *mutex, bool open)
{
    auto *logger = dynamic_cast<SyslogLogger *>(loggerMap.value("", nullptr));
    if (logger)
        return logger;

    // Need to add a new FileLogger
    mutex->unlock();
    // inserts into loggerMap
    logger = new SyslogLogger(open);
    mutex->lock();

    return logger;
}


/// \brief Process a log message, logging to syslog
/// \param item LoggingItem containing the log message to process
bool SyslogLogger::logmsg(LoggingItem *item)
{
    if (!m_opened || item->facility() <= 0)
        return false;

    char shortname = item->getLevelChar();
    syslog(item->level() | item->facility(), "%s[%d]: %c %s %s:%d (%s) %s",
           qPrintable(item->appName()), item->pid(), shortname,
           qPrintable(item->threadName()), qPrintable(item->file()), item->line(),
           qPrintable(item->function()), qPrintable(item->message()));

    return true;
}

#if CONFIG_SYSTEMD_JOURNAL
/// \brief JournalLogger constructor
JournalLogger::JournalLogger() :
    LoggerBase(nullptr)
{
    LOG(VB_GENERAL, LOG_INFO, "Added journal logging");
}

/// \brief JournalLogger deconstructor.
JournalLogger::~JournalLogger()
{
    LOG(VB_GENERAL, LOG_INFO, "Removing journal logging");
}

JournalLogger *JournalLogger::create(QMutex *mutex)
{
    auto *logger = dynamic_cast<JournalLogger *>(loggerMap.value("", nullptr));
    if (logger)
        return logger;

    // Need to add a new FileLogger
    mutex->unlock();
    // inserts into loggerMap
    logger = new JournalLogger();
    mutex->lock();

    return logger;
}


/// \brief Process a log message, logging to syslog
/// \param item LoggingItem containing the log message to process
bool JournalLogger::logmsg(LoggingItem *item)
{
    sd_journal_send(
        "MESSAGE=%s", qUtf8Printable(item->message()),
        "PRIORITY=%d", item->level(),
        "CODE_FILE=%s", qUtf8Printable(item->file()),
        "CODE_LINE=%d", item->line(),
        "CODE_FUNC=%s", qUtf8Printable(item->function()),
        "SYSLOG_IDENTIFIER=%s", qUtf8Printable(item->appName()),
        "SYSLOG_PID=%d", item->pid(),
        "MYTH_THREAD=%s", qUtf8Printable(item->threadName()),
        NULL
        );
    return true;
}
#endif
#endif

#ifndef _WIN32
/// \brief Signal handler for SIGHUP.  This passes it to the LogForwardThread
///        for processing.

void logSigHup(void)
{
    if (!logForwardThread)
        return;

    // This will be running in the thread that's used by SignalHandler
    // Emit the signal which is connected to a slot that runs in the actual
    // handling thread.
    emit logForwardThread->incomingSigHup();
}
#endif


/// \brief LogForwardThread constructor.
LogForwardThread::LogForwardThread() :
    MThread("LogForward")
{
    moveToThread(qthread());
}

/// \brief LogForwardThread destructor.
LogForwardThread::~LogForwardThread()
{
    stop();
    wait();
}

/// \brief Run the log forwarding thread.  This thread reads from an internal
///        queue and handles distributing the LoggingItems to each logger
///        instance vi ZeroMQ (inproc).
void LogForwardThread::run(void)
{
    RunProlog();

    connect(this, &LogForwardThread::incomingSigHup,
            this, &LogForwardThread::handleSigHup,
            Qt::QueuedConnection);

    qRegisterMetaType<QList<QByteArray> >("QList<QByteArray>");

    while (!m_aborted)
    {
        qApp->processEvents(QEventLoop::AllEvents, 10);
        qApp->sendPostedEvents(nullptr, QEvent::DeferredDelete);

        {
            QMutexLocker lock(&gLogItemListMutex);
            if (gLogItemList.isEmpty() &&
                !gLogItemListNotEmpty.wait(lock.mutex(), 90))
            {
                continue;
            }

            int processed = 0;
            while (!gLogItemList.isEmpty())
            {
                processed++;
                LoggingItem *item = gLogItemList.takeFirst();
                lock.unlock();
                forwardMessage(item);
                item->DecrRef();

                // Force a processEvents every 128 messages so a busy queue
                // doesn't preclude timer notifications, etc.
                if ((processed & 127) == 0)
                {
                    qApp->processEvents(QEventLoop::AllEvents, 10);
                    qApp->sendPostedEvents(nullptr, QEvent::DeferredDelete);
                }

                lock.relock();
            }
        }
    }

    LoggerList loggers;

    {
        QMutexLocker lock(&loggerMapMutex);
        loggers = loggerMap.values();
    }

    while (!loggers.isEmpty())
    {
        LoggerBase *logger = loggers.takeFirst();
        delete logger;
    }

    RunEpilog();
}


/// \brief  SIGHUP handler - reopen all open logfiles for logrollers
void LogForwardThread::handleSigHup(void)
{
#ifndef _WIN32
    LOG(VB_GENERAL, LOG_INFO, "SIGHUP received, rolling log files.");

    /* SIGHUP was sent.  Close and reopen debug logfiles */
    QMutexLocker locker(&loggerMapMutex);
    QMap<QString, LoggerBase *>::iterator it;
    for (it = loggerMap.begin(); it != loggerMap.end(); ++it)
    {
        it.value()->reopen();
    }
#endif
}

void LogForwardThread::forwardMessage(LoggingItem *item)
{
    QMutexLocker lock(&gLoggerListMutex);
    LoggerListItem *logItem = gLoggerList;

    if (logItem)
    {
        logItem->m_itemEpoch = nowAsDuration<std::chrono::seconds>();
    }
    else
    {
        QMutexLocker lock2(&loggerMapMutex);

        // Need to find or create the loggers
        auto *loggers = new LoggerList;

        // FileLogger from logFile
        QString logfile = item->logFile();
        if (!logfile.isEmpty())
        {
            LoggerBase *logger = FileLogger::create(logfile, lock2.mutex());

            if (logger && loggers)
                loggers->insert(0, logger);
        }

#ifndef _WIN32
        // SyslogLogger from facility
        int facility = item->facility();
        if (facility > 0)
        {
            LoggerBase *logger = SyslogLogger::create(lock2.mutex());

            if (logger && loggers)
                loggers->insert(0, logger);
        }

#if CONFIG_SYSTEMD_JOURNAL
        // Journal Logger
        if (facility == SYSTEMD_JOURNAL_FACILITY)
        {
            LoggerBase *logger = JournalLogger::create(lock2.mutex());

            if (logger && loggers)
                loggers->insert(0, logger);
        }
#endif
#endif

        // Add the list of loggers for this client into the map.
        logItem = new LoggerListItem;
        logItem->m_itemEpoch = nowAsDuration<std::chrono::seconds>();
        logItem->m_itemList = loggers;
        gLoggerList = logItem;
    }

    // Does this client have an entry in the loggers map, does that
    // entry have a list of loggers, and does that list have anything
    // in it.  I.E. is there anywhere to log this item.
    if (logItem->m_itemList && !logItem->m_itemList->isEmpty())
    {
        // Log this item on each of the loggers.
        for (auto *it : std::as_const(*logItem->m_itemList))
            it->logmsg(item);
    }
}

/// \brief Stop the thread by setting the abort flag
void LogForwardThread::stop(void)
{
    m_aborted = true;
}

bool logForwardStart(void)
{
    logForwardThread = new LogForwardThread();
    logForwardThread->start();

    usleep(10000);
    return (logForwardThread && logForwardThread->isRunning());
}

void logForwardStop(void)
{
    if (!logForwardThread)
        return;

    logForwardThread->stop();
    delete logForwardThread;
    logForwardThread = nullptr;
}

// Take a logging item and queue it for the logging server thread to
// process.
void logForwardMessage(LoggingItem *item)
{
    QMutexLocker lock(&gLogItemListMutex);

    bool wasEmpty = gLogItemList.isEmpty();
    item->IncrRef();
    gLogItemList.append(item);

    if (wasEmpty)
        gLogItemListNotEmpty.wakeAll();
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
