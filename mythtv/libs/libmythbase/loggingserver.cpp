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
using  ClientMap = QMap<QString, LoggerListItem *>;

using  ClientList = QList<QString>;
using  RevClientMap = QHash<LoggerBase *, ClientList *>;

static QMutex                       logClientMapMutex;
static ClientMap                    logClientMap;
static QAtomicInt                   logClientCount;

static QMutex                       logRevClientMapMutex;
static RevClientMap                 logRevClientMap;

static QMutex                       logMsgListMutex;
static LogMessageList               logMsgList;
static QWaitCondition               logMsgListNotEmpty;

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
        LoggerBase(filename)
{
    m_fd = open(filename, O_WRONLY|O_CREAT|O_APPEND, 0664);
    m_opened = (m_fd != -1);
    LOG(VB_GENERAL, LOG_INFO, QString("Added logging to %1")
             .arg(filename));
}


/// \brief FileLogger deconstructor - close the logfile
FileLogger::~FileLogger()
{
    if( m_opened )
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Removed logging to %1")
            .arg(m_handle));
        close(m_fd);
        m_fd = -1;
        m_opened = false;
    }
}

FileLogger *FileLogger::create(const QString& filename, QMutex *mutex)
{
    QByteArray ba = filename.toLocal8Bit();
    const char *file = ba.constData();
    auto *logger =
        qobject_cast<FileLogger *>(loggerMap.value(filename, nullptr));

    if (logger)
        return logger;

    // Need to add a new FileLogger
    mutex->unlock();
    // inserts into loggerMap
    logger = new FileLogger(file);
    mutex->lock();

    auto *clients = new ClientList;
    logRevClientMap.insert(logger, clients);
    return logger;
}

/// \brief Reopen the logfile after a SIGHUP.  Log files only (no console).
///        This allows for logrollers to be used.
void FileLogger::reopen(void)
{
    close(m_fd);

    m_fd = open(qPrintable(m_handle), O_WRONLY|O_CREAT|O_APPEND, 0664);
    m_opened = (m_fd != -1);
    LOG(VB_GENERAL, LOG_INFO, QString("Rolled logging on %1") .arg(m_handle));
}

/// \brief Process a log message, writing to the logfile
/// \param item LoggingItem containing the log message to process
bool FileLogger::logmsg(LoggingItem *item)
{
    if (!m_opened)
        return false;

    QString timestamp = item->getTimestampUs();
    QChar shortname = item->getLevelChar();

    std::string line;
    if( item->tid() )
    {
        line = qPrintable(QString("%1 %2 [%3/%4] %5 %6:%7 (%8) - %9\n")
                          .arg(timestamp, shortname,
                               QString::number(item->pid()),
                               QString::number(item->tid()),
                               item->threadName(), item->file(),
                               QString::number(item->line()),
                               item->function(),
                               item->message()));
    }
    else
    {
        line = qPrintable(QString("%1 %2 [%3] %5 %6:%7 (%8) - %9\n")
                          .arg(timestamp, shortname,
                               QString::number(item->pid()),
                               item->threadName(), item->file(),
                               QString::number(item->line()),
                               item->function(),
                               item->message()));
    }

    int result = write(m_fd, line.data(), line.size());

    if( result == -1 )
    {
        LOG(VB_GENERAL, LOG_ERR,
                 QString("Closed Log output on fd %1 due to errors").arg(m_fd));
        m_opened = false;
        close( m_fd );
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
    auto *logger = qobject_cast<SyslogLogger *>(loggerMap.value("", nullptr));
    if (logger)
        return logger;

    // Need to add a new FileLogger
    mutex->unlock();
    // inserts into loggerMap
    logger = new SyslogLogger(open);
    mutex->lock();

    auto *clients = new ClientList;
    logRevClientMap.insert(logger, clients);
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
    auto *logger = qobject_cast<JournalLogger *>(loggerMap.value("", nullptr));
    if (logger)
        return logger;

    // Need to add a new FileLogger
    mutex->unlock();
    // inserts into loggerMap
    logger = new JournalLogger();
    mutex->lock();

    auto *clients = new ClientList;
    logRevClientMap.insert(logger, clients);
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

/// \brief DatabaseLogger constructor
/// \param table C-string of the database table to log to
DatabaseLogger::DatabaseLogger(const char *table) :
    LoggerBase(table)
{
    m_query = QString(
        "INSERT INTO %1 "
        "    (`host`, `application`, `pid`, `tid`, `thread`, `filename`, "
        "     `line`, `function`, `msgtime`, `level`, `message`) "
        "VALUES (:HOST, :APP, :PID, :TID, :THREAD, :FILENAME, "
        "        :LINE, :FUNCTION, :MSGTIME, :LEVEL, :MESSAGE)")
        .arg(m_handle);

    LOG(VB_GENERAL, LOG_INFO, QString("Added database logging to table %1")
        .arg(m_handle));

    m_thread = new DBLoggerThread(this);
    m_thread->start();
}

/// \brief DatabaseLogger deconstructor
DatabaseLogger::~DatabaseLogger()
{
    LOG(VB_GENERAL, LOG_INFO, "Removing database logging");

    DatabaseLogger::stopDatabaseAccess();
}

DatabaseLogger *DatabaseLogger::create(const QString& table, QMutex *mutex)
{
    QByteArray ba = table.toLocal8Bit();
    const char *tble = ba.constData();
    auto *logger =
        qobject_cast<DatabaseLogger *>(loggerMap.value(table, nullptr));

    if (logger)
        return logger;

    // Need to add a new FileLogger
    mutex->unlock();
    // inserts into loggerMap
    logger = new DatabaseLogger(tble);
    mutex->lock();

    auto *clients = new ClientList;
    logRevClientMap.insert(logger, clients);
    return logger;
}

/// \brief Stop logging to the database and wait for the thread to stop.
void DatabaseLogger::stopDatabaseAccess(void)
{
    if( m_thread )
    {
        m_thread->stop();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
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
        m_disabledTime.start();
    }

    if (!m_disabledTime.isValid() && m_thread->queueFull())
    {
        m_disabledTime.start();
        LOG(VB_GENERAL, LOG_CRIT,
            "Disabling DB Logging: too many messages queued");
        return false;
    }

    if (m_disabledTime.isValid() && m_disabledTime.hasExpired(kMinDisabledTime.count()))
    {
        if (isDatabaseReady() && !m_thread->queueFull())
        {
            m_disabledTime.invalidate();
            LOG(VB_GENERAL, LOG_CRIT, "Reenabling DB Logging");
        }
    }

    if (m_disabledTime.isValid())
        return false;

    m_thread->enqueue(item);
    return true;
}


/// \brief Actually insert a log message from the queue into the database
/// \param query    The database insert query to use
/// \param item     LoggingItem containing the log message to insert
bool DatabaseLogger::logqmsg(MSqlQuery &query, LoggingItem *item)
{
    QString timestamp = item->getTimestamp();

    query.bindValue(":TID",         item->tid());
    query.bindValue(":THREAD",      item->threadName());
    query.bindValue(":FILENAME",    item->file());
    query.bindValue(":LINE",        item->line());
    query.bindValue(":FUNCTION",    item->function());
    query.bindValue(":MSGTIME",     timestamp);
    query.bindValue(":LEVEL",       item->level());
    query.bindValue(":MESSAGE",     item->message());
    query.bindValue(":APP",         item->appName());
    query.bindValue(":PID",         item->pid());

    if (!query.exec())
    {
        // Suppress Driver not loaded errors that occur at startup.
        // and suppress additional errors for one second after the
        // previous error (to avoid spamming the log).
        QSqlError err = query.lastError();
        if ((err.type() != 1
             || !err.nativeErrorCode().isEmpty()
                ) &&
            (!m_errorLoggingTime.isValid() ||
             (m_errorLoggingTime.hasExpired(1000))))
        {
            MythDB::DBError("DBLogging", query);
            m_errorLoggingTime.start();
        }
        return false;
    }

    return true;
}

/// \brief Prepare the database query for use, and bind constant values to it.
/// \param query    The database query to prepare
void DatabaseLogger::prepare(MSqlQuery &query)
{
    query.prepare(m_query);
    query.bindValue(":HOST", gCoreContext->GetHostName());
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
            m_loggingTableExists = tableExists(m_handle);

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
        QString sql = "SELECT COLUMN_NAME "
                      "  FROM INFORMATION_SCHEMA.COLUMNS "
                      " WHERE TABLE_SCHEMA = DATABASE() "
                      "   AND TABLE_NAME = :TABLENAME "
                      "   AND COLUMN_NAME = :COLUMNNAME;";
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
    MThread("DBLogger"), m_logger(logger),
    m_queue(new QQueue<LoggingItem *>),
    m_wait(new QWaitCondition())
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
        m_queue->dequeue()->DecrRef();
    delete m_queue;
    delete m_wait;
    m_queue = nullptr;
    m_wait = nullptr;
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
        if ((m_aborted || (gCoreContext && m_logger->isDatabaseReady())))
            break;

        QMutexLocker locker(&m_queueMutex);
        m_wait->wait(locker.mutex(), 100);
    }

    if (!m_aborted)
    {
        // We want the query to be out of scope before the RunEpilog() so
        // shutdown occurs correctly as otherwise the connection appears still
        // in use, and we get a qWarning on shutdown.
        auto *query = new MSqlQuery(MSqlQuery::InitCon());
        m_logger->prepare(*query);

        QMutexLocker qLock(&m_queueMutex);
        while (!m_aborted || !m_queue->isEmpty())
        {
            if (m_queue->isEmpty())
            {
                m_wait->wait(qLock.mutex(), 100);
                continue;
            }

            LoggingItem *item = m_queue->dequeue();
            if (!item)
                continue;

            if (!item->message().isEmpty())
            {
                qLock.unlock();
                bool logged = m_logger->logqmsg(*query, item);
                qLock.relock();

                if (!logged)
                {
                    m_queue->prepend(item);
                    m_wait->wait(qLock.mutex(), 100);
                    delete query;
                    query = new MSqlQuery(MSqlQuery::InitCon());
                    m_logger->prepare(*query);
                    continue;
                }
            }

            item->DecrRef();
        }

        delete query;
    }

    RunEpilog();
}

/// \brief Tell the thread to stop by setting the m_aborted flag.
void DBLoggerThread::stop(void)
{
    QMutexLocker qLock(&m_queueMutex);
    m_aborted = true;
    m_wait->wakeAll();
}

bool DBLoggerThread::enqueue(LoggingItem *item)
{
    QMutexLocker qLock(&m_queueMutex);
    if (!m_aborted)
    {
        if (item)
        {
            item->IncrRef();
        }
        m_queue->enqueue(item);
    }
    return true;
}


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
            QMutexLocker lock(&logMsgListMutex);
            if (logMsgList.isEmpty() &&
                !logMsgListNotEmpty.wait(lock.mutex(), 90))
            {
                continue;
            }

            int processed = 0;
            while (!logMsgList.isEmpty())
            {
                processed++;
                LogMessage *msg = logMsgList.takeFirst();
                lock.unlock();
                forwardMessage(msg);
                delete msg;

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

void LogForwardThread::forwardMessage(LogMessage *msg)
{
#ifdef DUMP_PACKET
    QList<QByteArray>::const_iterator it = msg->begin();
    int i = 0;
    for (; it != msg->end(); ++it, i++)
    {
        QByteArray buf = *it;
        cout << i << ":\t" << buf.size() << endl << "\t"
             << buf.toHex().constData() << endl << "\t"
             << buf.constData() << endl;
    }
#endif

    // First section is the client id
    QByteArray clientBa = msg->first();
    QString clientId = QString(clientBa.toHex());

    QByteArray json     = msg->at(1);

    if (json.size() == 0)
    {
        // cout << "invalid msg, no json data " << qPrintable(clientId) << endl;
        return;
    }

    QMutexLocker lock(&logClientMapMutex);
    LoggerListItem *logItem = logClientMap.value(clientId, nullptr);

    // cout << "msg  " << clientId.toLocal8Bit().constData() << endl;
    if (logItem)
    {
        logItem->m_itemEpoch = nowAsDuration<std::chrono::seconds>();
    }
    else
    {
        LoggingItem *item = LoggingItem::create(json);

        logClientCount.ref();
        LOG(VB_FILE, LOG_DEBUG, QString("New Logging Client: ID: %1 (#%2)")
            .arg(clientId).arg(logClientCount.fetchAndAddOrdered(0)));

        QMutexLocker lock2(&loggerMapMutex);
        QMutexLocker lock3(&logRevClientMapMutex);

        // Need to find or create the loggers
        auto *loggers = new LoggerList;

        // FileLogger from logFile
        QString logfile = item->logFile();
        if (!logfile.isEmpty())
        {
            LoggerBase *logger = FileLogger::create(logfile, lock2.mutex());

            ClientList *clients = logRevClientMap.value(logger);

            if (clients)
                clients->insert(0, clientId);

            if (logger && loggers)
                loggers->insert(0, logger);
        }

#ifndef _WIN32
        // SyslogLogger from facility
        int facility = item->facility();
        if (facility > 0)
        {
            LoggerBase *logger = SyslogLogger::create(lock2.mutex());

            ClientList *clients = logRevClientMap.value(logger);

            if (clients)
                clients->insert(0, clientId);

            if (logger && loggers)
                loggers->insert(0, logger);
        }

#if CONFIG_SYSTEMD_JOURNAL
        // Journal Logger
        if (facility == SYSTEMD_JOURNAL_FACILITY)
        {
            LoggerBase *logger = JournalLogger::create(lock2.mutex());

            ClientList *clients = logRevClientMap.value(logger);

            if (clients)
                clients->insert(0, clientId);

            if (logger && loggers)
                loggers->insert(0, logger);
        }
#endif
#endif

        // DatabaseLogger from table
        QString table = item->table();
        if (!table.isEmpty())
        {
            LoggerBase *logger = DatabaseLogger::create(table, lock2.mutex());

            ClientList *clients = logRevClientMap.value(logger);

            if (clients)
                clients->insert(0, clientId);

            if (logger && loggers)
                loggers->insert(0, logger);
        }

        logItem = new LoggerListItem;
        logItem->m_itemEpoch = nowAsDuration<std::chrono::seconds>();
        logItem->m_itemList = loggers;
        logClientMap.insert(clientId, logItem);

        item->DecrRef();
    }

    if (logItem && logItem->m_itemList && !logItem->m_itemList->isEmpty())
    {
        LoggingItem *item = LoggingItem::create(json);
        if (!item)
            return;
        for (auto *it : qAsConst(*logItem->m_itemList))
            it->logmsg(item);
        item->DecrRef();
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

    QMutexLocker locker(&loggerMapMutex);
    for (auto it = loggerMap.begin(); it != loggerMap.end(); ++it)
    {
        it.value()->stopDatabaseAccess();
    }
}

void logForwardMessage(const QList<QByteArray> &msg)
{
    auto *message = new LogMessage(msg);
    QMutexLocker lock(&logMsgListMutex);

    bool wasEmpty = logMsgList.isEmpty();
    logMsgList.append(message);

    if (wasEmpty)
        logMsgListNotEmpty.wakeAll();
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
