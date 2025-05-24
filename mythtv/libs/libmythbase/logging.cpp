#include <QtGlobal>
#include <QAtomicInt>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QList>
#include <QQueue>
#include <QHash>
#include <QFileInfo>
#include <QStringList>
#include <QMap>
#include <QRegularExpression>
#include <QVariantMap>
#include <iostream>

#include "mythlogging.h"
#include "logging.h"
#include "loggingserver.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mythsystemlegacy.h"
#include "dbutil.h"
#include "exitcodes.h"
#include "compat.h"

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>
#if HAVE_GETTIMEOFDAY
#include <sys/time.h>
#endif
#define SYSLOG_NAMES
#ifndef _WIN32
#include "mythsyslog.h"
#endif
#include <unistd.h>

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

#ifdef Q_OS_ANDROID
#include <android/log.h>
#endif

static QMutex                  logQueueMutex;
static QQueue<LoggingItem *>   logQueue;

static LoggerThread           *logThread = nullptr;
static QMutex                  logThreadMutex;
static QHash<uint64_t, QString> logThreadHash;

static QMutex                   logThreadTidMutex;
static QHash<uint64_t, int64_t> logThreadTidHash;

static bool                    logThreadFinished = false;
static bool                    debugRegistration = false;

struct LogPropagateOpts {
    bool    m_propagate;
    int     m_quiet;
    int     m_facility;
    QString m_path;
    bool    m_loglong;
};

LogPropagateOpts        logPropagateOpts {false, 0, 0, "", false};
QString                 logPropagateArgs;
QStringList             logPropagateArgList;

LogLevel_t logLevel = LOG_INFO;

bool verboseInitialized = false;
VerboseMap verboseMap;
QMutex verboseMapMutex;

LoglevelMap loglevelMap;
QMutex loglevelMapMutex;

const uint64_t verboseDefaultInt = VB_GENERAL;
const QString verboseDefaultStr { QStringLiteral(" general") };

uint64_t verboseMask = verboseDefaultInt;
QString verboseString = verboseDefaultStr;
ComponentLogLevelMap componentLogLevel;

uint64_t     userDefaultValueInt = verboseDefaultInt;
QString      userDefaultValueStr = verboseDefaultStr;
bool         haveUserDefaultValues = false;

void verboseAdd(uint64_t mask, QString name, bool additive, QString helptext);
void loglevelAdd(int value, QString name, char shortname);
void verboseInit(void);
void verboseHelp(void);

/// \brief Intended for use only by the test harness.
void resetLogging(void)
{
    verboseMask = verboseDefaultInt;
    verboseString = verboseDefaultStr;
    userDefaultValueInt = verboseDefaultInt;
    userDefaultValueStr = verboseDefaultStr;
    haveUserDefaultValues = false;

    verboseInit();
}

LoggingItem::LoggingItem(const char *_file, const char *_function,
                         int _line, LogLevel_t _level, LoggingType _type) :
        ReferenceCounter("LoggingItem", false),
        m_threadId((uint64_t)(QThread::currentThreadId())),
        m_line(_line), m_type(_type), m_level(_level),
        m_function(_function)
{
    const char *slash = std::strrchr(_file, '/');
    m_file = (slash != nullptr) ? slash+1 : _file;
    m_epoch = nowAsDuration<std::chrono::microseconds>();
    setThreadTid();
}

/// \brief Get the name of the thread that produced the LoggingItem
/// \return C-string of the thread name
QString LoggingItem::getThreadName(void)
{
    static constexpr char const *kSUnknown = "thread_unknown";

    if( !m_threadName.isEmpty() )
        return m_threadName;

    QMutexLocker locker(&logThreadMutex);
    return logThreadHash.value(m_threadId, kSUnknown);
}

/// \brief Get the thread ID of the thread that produced the LoggingItem
/// \return Thread ID of the producing thread, cast to a 64-bit signed integer
/// \note  In different platforms, the actual value returned here will vary.
///        The intention is to get a thread ID that will map well to what is
///        shown in gdb.
int64_t LoggingItem::getThreadTid(void)
{
    QMutexLocker locker(&logThreadTidMutex);
    m_tid = logThreadTidHash.value(m_threadId, 0);
    return m_tid;
}

/// \brief Set the thread ID of the thread that produced the LoggingItem.  This
///        code is actually run in the thread in question as part of the call
///        to LOG()
/// \note  In different platforms, the actual value returned here will vary.
///        The intention is to get a thread ID that will map well to what is
///        shown in gdb.
void LoggingItem::setThreadTid(void)
{
    QMutexLocker locker(&logThreadTidMutex);

    m_tid = logThreadTidHash.value(m_threadId, -1);
    if (m_tid == -1)
    {
        m_tid = 0;

#if defined(Q_OS_ANDROID)
        m_tid = (int64_t)gettid();
#elif defined(__linux__)
        m_tid = syscall(SYS_gettid);
#elif defined(__FreeBSD__)
        long lwpid;
        [[maybe_unused]] int dummy = thr_self( &lwpid );
        m_tid = (int64_t)lwpid;
#elif defined(Q_OS_DARWIN)
        m_tid = (int64_t)mach_thread_self();
#endif
        logThreadTidHash[m_threadId] = m_tid;
    }
}

/// \brief Convert numerical timestamp to a readable date and time.
QString LoggingItem::getTimestamp (const char *format) const
{
    QDateTime epoch = QDateTime::fromMSecsSinceEpoch(m_epoch.count()/1000);
    QString timestamp = epoch.toString(format);
    return timestamp;
}

QString LoggingItem::getTimestampUs (const char *format) const
{
    QString timestamp = getTimestamp(format);
    timestamp += QString(".%1").arg((m_epoch % 1s).count(),6,10,QChar('0'));
    return timestamp;
}

/// \brief Get the message log level as a single character.
char LoggingItem::getLevelChar (void)
{
    QMutexLocker locker(&loglevelMapMutex);
    LoglevelMap::iterator it = loglevelMap.find(m_level);
    if (it != loglevelMap.end())
        return (*it)->shortname;
    return '-';
}

std::string LoggingItem::toString()
{
    QString ptid = QString::number(pid()); // pid, add tid if non-zero
    if(tid())
    {
        ptid.append("/").append(QString::number(tid()));
    }
    return qPrintable(QString("%1 %2 [%3] %4 %5:%6:%7  %8\n")
                          .arg(getTimestampUs(),
                               QString(QChar(getLevelChar())),
                               ptid,
                               threadName(),
                               file(),
                               QString::number(line()),
                               function(),
                               message()
                              ));
}

std::string LoggingItem::toStringShort()
{
    return qPrintable(QString("%1 %2  %3\n")
                          .arg(getTimestampUs(),
                               QString(QChar(getLevelChar())),
                               message()
                              ));
}

/// \brief LoggerThread constructor.  Enables debugging of thread registration
///        and deregistration if the VERBOSE_THREADS environment variable is
///        set.
LoggerThread::LoggerThread(QString filename, bool progress, bool quiet,
                           int facility, bool loglong) :
    MThread("Logger"),
    m_waitNotEmpty(new QWaitCondition()),
    m_waitEmpty(new QWaitCondition()),
    m_filename(std::move(filename)), m_progress(progress), m_quiet(quiet),
    m_loglong(loglong),
    m_facility(facility), m_pid(getpid())
{
    if (qEnvironmentVariableIsSet("VERBOSE_THREADS"))
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "Logging thread registration/deregistration enabled!");
        debugRegistration = true;
    }

    if (!logForwardStart())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Failed to start LogServer thread");
    }
    moveToThread(qthread());
}

/// \brief LoggerThread destructor.  Triggers the deletion of all loggers.
LoggerThread::~LoggerThread()
{
    stop();
    wait();
    logForwardStop();

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

    LOG(VB_GENERAL, LOG_INFO, "Added logging to the console");

    bool dieNow = false;

    QMutexLocker qLock(&logQueueMutex);

    while (!m_aborted || !logQueue.isEmpty())
    {
        qLock.unlock();
        qApp->processEvents(QEventLoop::AllEvents, 10);
        qApp->sendPostedEvents(nullptr, QEvent::DeferredDelete);

        qLock.relock();
        if (logQueue.isEmpty())
        {
            m_waitEmpty->wakeAll();
            m_waitNotEmpty->wait(qLock.mutex(), 100);
            continue;
        }

        LoggingItem *item = logQueue.dequeue();
        qLock.unlock();

        fillItem(item);
        handleItem(item);
        logConsole(item);
        item->DecrRef();

        qLock.relock();
    }

    qLock.unlock();

    // This must be before the timer stop below or we deadlock when the timer
    // thread tries to deregister, and we wait for it.
    logThreadFinished = true;

    RunEpilog();

    if (dieNow)
    {
        qApp->processEvents();
    }
}

/// \brief  Handles each LoggingItem.  There is a special case for
///         thread registration and deregistration which are also included in
///         the logging queue to keep the thread names in sync with the log
///         messages.
/// \param  item    The LoggingItem to be handled
void LoggerThread::handleItem(LoggingItem *item)
{
    if (item->m_type & kRegistering)
    {
        item->m_tid = item->getThreadTid();

        QMutexLocker locker(&logThreadMutex);
        logThreadHash[item->m_threadId] = item->m_threadName;

        if (debugRegistration)
        {
            item->m_message = QString("Thread 0x%1 (%2) registered as \'%3\'")
                .arg(QString::number(item->m_threadId,16),
                     QString::number(item->m_tid),
                     logThreadHash[item->m_threadId]);
        }
    }
    else if (item->m_type & kDeregistering)
    {
        int64_t tid = 0;

        {
            QMutexLocker locker(&logThreadTidMutex);
            if( logThreadTidHash.contains(item->m_threadId) )
            {
                tid = logThreadTidHash[item->m_threadId];
                logThreadTidHash.remove(item->m_threadId);
            }
        }

        QMutexLocker locker(&logThreadMutex);
        if (logThreadHash.contains(item->m_threadId))
        {
            if (debugRegistration)
            {
                item->m_message = QString("Thread 0x%1 (%2) deregistered as \'%3\'")
                    .arg(QString::number(item->m_threadId,16),
                         QString::number(tid),
                         logThreadHash[item->m_threadId]);
            }
            logThreadHash.remove(item->m_threadId);
        }
    }

    if (!item->m_message.isEmpty())
    {
        logForwardMessage(item);
    }
}

/// \brief Process a log message, writing to the console
/// \param item LoggingItem containing the log message to process
bool LoggerThread::logConsole(LoggingItem *item) const
{
    if (m_quiet || (m_progress && item->m_level > LOG_ERR))
        return false;

    if (!(item->m_type & kMessage))
        return false;

    item->IncrRef();

#ifndef Q_OS_ANDROID
    std::string line;

    if (item->m_type & kStandardIO)
    {
        line = qPrintable(item->m_message);
    }
    else
    {
#if !defined(NDEBUG) || CONFIG_FORCE_LOGLONG
        if (true)
#else
        if (m_loglong)
#endif
        {
            line = item->toString();
        }
        else
        {
            line = item->toStringShort();
        }
    }

    std::cout << line << std::flush;

#else // Q_OS_ANDROID

    android_LogPriority aprio;
    switch (item->m_level)
    {
    case LOG_EMERG:
        aprio = ANDROID_LOG_FATAL;
        break;
    case LOG_ALERT:
    case LOG_CRIT:
    case LOG_ERR:
        aprio = ANDROID_LOG_ERROR;
        break;
    case LOG_WARNING:
        aprio = ANDROID_LOG_WARN;
        break;
    case LOG_NOTICE:
    case LOG_INFO:
        aprio = ANDROID_LOG_INFO;
        break;
    case LOG_DEBUG:
        aprio = ANDROID_LOG_DEBUG;
        break;
    case LOG_UNKNOWN:
    default:
        aprio = ANDROID_LOG_UNKNOWN;
        break;
    }
#ifndef NDEBUG
    __android_log_print(aprio, "mfe", "%s:%d:%s  %s", qPrintable(item->m_file),
                        item->m_line, qPrintable(item->m_function),
                        qPrintable(item->m_message));
#else
    __android_log_print(aprio, "mfe", "%s", qPrintable(item->m_message));
#endif
#endif

    item->DecrRef();

    return true;
}


/// \brief Stop the thread by setting the abort flag after waiting a second for
///        the queue to be flushed.
void LoggerThread::stop(void)
{
    logQueueMutex.lock();
    flush(1000);
    m_aborted = true;
    logQueueMutex.unlock();
    m_waitNotEmpty->wakeAll();
}

/// \brief  Wait for the queue to be flushed (up to a timeout)
/// \param  timeoutMS   The number of ms to wait for the queue to flush
/// \return true if the queue is empty, false otherwise
bool LoggerThread::flush(int timeoutMS)
{
    QElapsedTimer t;
    t.start();
    while (!m_aborted && !logQueue.isEmpty() && !t.hasExpired(timeoutMS))
    {
        m_waitNotEmpty->wakeAll();
        int left = timeoutMS - t.elapsed();
        if (left > 0)
            m_waitEmpty->wait(&logQueueMutex, left);
    }
    return logQueue.isEmpty();
}

void LoggerThread::fillItem(LoggingItem *item)
{
    if (!item)
        return;

    item->setPid(m_pid);
    item->setThreadName(item->getThreadName());
    item->setAppName(m_appname);
    item->setLogFile(m_filename);
    item->setFacility(m_facility);
}


/// \brief  Create a new LoggingItem
/// \param  _file   filename of the source file where the log message is from
/// \param  _function source function where the log message is from
/// \param  _line   line number in the source where the log message is from
/// \param  _level  logging level of the message (LogLevel_t)
/// \param  _type   type of logging message
/// \return LoggingItem that was created
LoggingItem *LoggingItem::create(const char *_file,
                                 const char *_function,
                                 int _line, LogLevel_t _level,
                                 LoggingType _type)
{
    auto *item = new LoggingItem(_file, _function, _line, _level, _type);

    return item;
}

/// \brief  Format and send a log message into the queue.  This is called from
///         the LOG() macro.  The intention is minimal blocking of the caller.
/// \param  mask    Verbosity mask of the message (VB_*)
/// \param  level   Log level of this message (LOG_* - matching syslog levels)
/// \param  file    Filename of source code logging the message
/// \param  line    Line number within the source of log message source
/// \param  function    Function name of the log message source
/// \param  message     log message
void LogPrintLine( uint64_t mask, LogLevel_t level, const char *file, int line,
                   const char *function, QString message)
{
    int type = kMessage;
    type |= (mask & VB_FLUSH) ? kFlush : 0;
    type |= (mask & VB_STDIO) ? kStandardIO : 0;
    LoggingItem *item = LoggingItem::create(file, function, line, level,
                                            (LoggingType)type);
    if (!item)
        return;

    item->m_message = std::move(message);

    QMutexLocker qLock(&logQueueMutex);

#if defined( _MSC_VER ) && defined( _DEBUG )
        OutputDebugStringA( qPrintable(item->m_message) );
        OutputDebugStringA( "\n" );
#endif

    logQueue.enqueue(item);

    if (logThread && logThreadFinished && !logThread->isRunning())
    {
        while (!logQueue.isEmpty())
        {
            item = logQueue.dequeue();
            qLock.unlock();
            LoggerThread::handleItem(item);
            logThread->logConsole(item);
            item->DecrRef();
            qLock.relock();
        }
    }
    else if (logThread && !logThreadFinished && (type & kFlush))
    {
        logThread->flush();
    }
}


/// \brief Generate the logPropagateArgs global with the latest logging
///        level, mask, etc to propagate to all of the mythtv programs
///        spawned from this one.
void logPropagateCalc(void)
{
    logPropagateArgList.clear();

    QString mask = verboseString.simplified().replace(' ', ',');
    logPropagateArgs = " --verbose " + mask;
    logPropagateArgList << "--verbose" << mask;

    if (logPropagateOpts.m_propagate)
    {
        logPropagateArgs += " --logpath " + logPropagateOpts.m_path;
        logPropagateArgList << "--logpath" << logPropagateOpts.m_path;
    }

    QString name = logLevelGetName(logLevel);
    logPropagateArgs += " --loglevel " + name;
    logPropagateArgList << "--loglevel" << name;

    for (int i = 0; i < logPropagateOpts.m_quiet; i++)
    {
        logPropagateArgs += " --quiet";
        logPropagateArgList << "--quiet";
    }

    if (logPropagateOpts.m_loglong)
    {
        logPropagateArgs += " --loglong";
        logPropagateArgList << "--loglong";
    }

#if !defined(_WIN32) && !defined(Q_OS_ANDROID)
    if (logPropagateOpts.m_facility >= 0)
    {
        const CODE *syslogname = nullptr;
        for (syslogname = &facilitynames[0];
             (syslogname->c_name &&
              syslogname->c_val != logPropagateOpts.m_facility); syslogname++);

        logPropagateArgs += QString(" --syslog %1").arg(syslogname->c_name);
        logPropagateArgList << "--syslog" << syslogname->c_name;
    }
#if CONFIG_SYSTEMD_JOURNAL
    else if (logPropagateOpts.m_facility == SYSTEMD_JOURNAL_FACILITY)
    {
        logPropagateArgs += " --systemd-journal";
        logPropagateArgList << "--systemd-journal";
    }
#endif
#endif
}

/// \brief Check if we are propagating a "--quiet"
/// \return true if --quiet is being propagated
bool logPropagateQuiet(void)
{
    return logPropagateOpts.m_quiet != 0;
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
/// \param  propagate   true if the logfile path needs to be propagated to child
///                     processes.
/// \param  testHarness Should always be false. Set to true when
///                     invoked by the testing code.
void logStart(const QString& logfile, bool progress, int quiet, int facility,
              LogLevel_t level, bool propagate, bool loglong, bool testHarness)
{
    if (logThread && logThread->isRunning())
        return;

    logLevel = level;
    LOG(VB_GENERAL, LOG_NOTICE, QString("Setting Log Level to LOG_%1")
             .arg(logLevelGetName(logLevel).toUpper()));

    logPropagateOpts.m_propagate = propagate;
    logPropagateOpts.m_quiet = quiet;
    logPropagateOpts.m_facility = facility;
    logPropagateOpts.m_loglong = loglong;

    if (propagate)
    {
        QFileInfo finfo(logfile);
        QString path = finfo.path();
        logPropagateOpts.m_path = path;
    }

    logPropagateCalc();
    if (testHarness)
        return;

    if (!logThread)
        logThread = new LoggerThread(logfile, progress, quiet, facility, loglong);

    logThread->start();
}

/// \brief  Entry point for stopping logging for an application
void logStop(void)
{
    if (logThread)
    {
        logThread->stop();
        logThread->wait();
        qDeleteAll(verboseMap);  // delete VerboseDef memory in map values
        verboseMap.clear();
        qDeleteAll(loglevelMap); // delete LoglevelDef memory in map values
        loglevelMap.clear();
        delete logThread;
        logThread = nullptr;
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

    LoggingItem *item = LoggingItem::create(__FILE__, __FUNCTION__,
                                            __LINE__, LOG_DEBUG,
                                            kRegistering);
    if (item)
    {
        item->setThreadName((char *)name.toLocal8Bit().constData());
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

    LoggingItem *item = LoggingItem::create(__FILE__, __FUNCTION__, __LINE__,
                                            LOG_DEBUG,
                                            kDeregistering);
    if (item)
        logQueue.enqueue(item);
}


/// \brief  Map a syslog facility name back to the enumerated value
/// \param  facility    QString containing the facility name
/// \return Syslog facility as enumerated type.  Negative if not found.
int syslogGetFacility([[maybe_unused]] const QString& facility)
{
#ifdef _WIN32
    LOG(VB_GENERAL, LOG_NOTICE,
        "Windows does not support syslog, disabling" );
    return( -2 );
#elif defined(Q_OS_ANDROID)
    LOG(VB_GENERAL, LOG_NOTICE,
        "Android does not support syslog, disabling" );
    return( -2 );
#else
    const CODE *name = nullptr;
    QByteArray ba = facility.toLocal8Bit();
    char *string = (char *)ba.constData();

    for (name = &facilitynames[0];
         name->c_name && (strcmp(name->c_name, string) != 0); name++);

    return( name->c_val );
#endif
}

/// \brief  Map a log level name back to the enumerated value
/// \param  level   QString containing the log level name
/// \return Log level as enumerated type.  LOG_UNKNOWN if not found.
LogLevel_t logLevelGet(const QString& level)
{
    QMutexLocker locker(&loglevelMapMutex);
    if (!verboseInitialized)
    {
        locker.unlock();
        verboseInit();
        locker.relock();
    }

    for (auto *item : std::as_const(loglevelMap))
    {
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
        return {"unknown"};

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
    auto *item = new VerboseDef;

    item->mask = mask;
    // VB_GENERAL -> general
    name.remove(0, 3);
    name = name.toLower();
    item->name = name;
    item->additive = additive;
    item->helpText = std::move(helptext);

    verboseMap.insert(name, item);
}

/// \brief  Add a log level to the logLevelMap.  Done at initialization.
/// \param  value       log level enumerated value (LOG_*) - matches syslog
///                     levels
/// \param  name        name of the log level
/// \param  shortname   one-letter short name for output into logs
void loglevelAdd(int value, QString name, char shortname)
{
    auto *item = new LoglevelDef;

    item->value = value;
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
    qDeleteAll(verboseMap);  // delete VerboseDef memory in map values
    verboseMap.clear();
    qDeleteAll(loglevelMap); // delete LoglevelDef memory in map values
    loglevelMap.clear();

    // This looks funky, so I'll put some explanation here.  The verbosedefs.h
    // file gets included as part of the mythlogging.h include, and at that
    // time, the normal (without MYTH_IMPLEMENT_VERBOSE defined) code case will
    // define the VerboseMask enum.  At this point, we force it to allow us to
    // include the file again, but with MYTH_IMPLEMENT_VERBOSE set so that the
    // single definition of the VB_* values can be shared to define also the
    // contents of verboseMap, via repeated calls to verboseAdd()

#undef VERBOSEDEFS_H_
#define MYTH_IMPLEMENT_VERBOSE
#include "verbosedefs.h"

    verboseInitialized = true;
}


/// \brief Outputs the Verbose levels and their descriptions
///        (for --verbose help)
void verboseHelp(void)
{
    QString m_verbose = userDefaultValueStr.simplified().replace(' ', ',');

    std::cerr << "Verbose debug levels.\n"
            "Accepts any combination (separated by comma) of:\n\n";

    for (VerboseMap::Iterator vit = verboseMap.begin();
         vit != verboseMap.end(); ++vit )
    {
        VerboseDef *item = vit.value();
        QString name = QString("  %1").arg(item->name, -15, ' ');
        if (item->helpText.isEmpty())
            continue;
        std::cerr << name.toLocal8Bit().constData() << " - "
                  << item->helpText.toLocal8Bit().constData() << std::endl;
    }

    std::cerr << std::endl <<
      "The default for this program appears to be: '-v " <<
      m_verbose.toLocal8Bit().constData() << "'\n\n"
      "Most options are additive except for 'none' and 'all'.\n"
      "These two are semi-exclusive and take precedence over any\n"
      "other options.  However, you may use something like\n"
      "'-v none,jobqueue' to receive only JobQueue related messages\n"
      "and override the default verbosity level.\n\n"
      "Additive options may also be subtracted from 'all' by\n"
      "prefixing them with 'no', so you may use '-v all,nodatabase'\n"
      "to view all but database debug messages.\n\n";

    std::cerr
         << "The 'global' loglevel is specified with --loglevel, but can be\n"
         << "overridden on a component by component basis by appending "
         << "':level'\n"
         << "to the component.\n"
         << "    For example: -v gui:debug,channel:notice,record\n\n";

    std::cerr << "Some debug levels may not apply to this program.\n" << std::endl;
}

/// \brief  Parse the --verbose commandline argument and set the verbose level
/// \param  arg the commandline argument following "--verbose"
/// \return an exit code.  GENERIC_EXIT_OK if all is well.
int verboseArgParse(const QString& arg)
{
    QString option;

    if (!verboseInitialized)
        verboseInit();

    QMutexLocker locker(&verboseMapMutex);

    verboseMask = verboseDefaultInt;
    verboseString = verboseDefaultStr;

    if (arg.startsWith('-'))
    {
        std::cerr << "Invalid or missing argument to -v/--verbose option\n";
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    static const QRegularExpression kSeparatorRE { "[^\\w:]+" };
    QStringList verboseOpts = arg.split(kSeparatorRE, Qt::SkipEmptyParts);
    for (const auto& opt : std::as_const(verboseOpts))
    {
        option = opt.toLower();
        bool reverseOption = false;
        QString optionLevel;

        if (option != "none" && option.startsWith("no"))
        {
            reverseOption = true;
            option = option.right(option.length() - 2);
        }

        if (option == "help")
        {
            verboseHelp();
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        if (option == "important")
        {
            std::cerr << "The \"important\" log mask is no longer valid.\n";
        }
        else if (option == "extra")
        {
            std::cerr << "The \"extra\" log mask is no longer valid.  Please try "
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
                verboseString = verboseDefaultStr;
            }
        }
        else
        {
            int idx = option.indexOf(':');
            if (idx != -1)
            {
                optionLevel = option.mid(idx + 1);
                option = option.left(idx);
            }

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

                    if (!optionLevel.isEmpty())
                    {
                        LogLevel_t level = logLevelGet(optionLevel);
                        if (level != LOG_UNKNOWN)
                            componentLogLevel[item->mask] = level;
                    }
                }
            }
            else
            {
                std::cerr << "Unknown argument for -v/--verbose: " <<
                        option.toLocal8Bit().constData() << std::endl;;
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
