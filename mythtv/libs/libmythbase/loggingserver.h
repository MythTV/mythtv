#ifndef LOGGINGSERVER_H_
#define LOGGINGSERVER_H_

#include <QMutexLocker>
#include <QSocketNotifier>
#include <QMutex>
#include <QQueue>
#include <QElapsedTimer>

#include <cstdint>
#include <fstream>
#include <unistd.h>

#include "mythconfig.h"
#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.
#include "verbosedefs.h"
#include "mthread.h"

class QString;
class MSqlQuery;
class LoggingItem;

/// \brief Base class for the various logging mechanisms
class LoggerBase
{
  public:
    /// \brief LoggerBase Constructor
    explicit LoggerBase(const char *string);
    /// \brief LoggerBase Deconstructor
    virtual ~LoggerBase();
    /// \brief Process a log message for the logger instance
    /// \param item LoggingItem containing the log message to process
    virtual bool logmsg(LoggingItem *item) = 0;
    /// \brief Reopen the log file to facilitate log rolling
    virtual void reopen(void) = 0;
  protected:
    QString m_handle; ///< semi-opaque handle for identifying instance
};

/// \brief File-based logger - used for logfiles and console
class FileLogger : public LoggerBase
{
  public:
    explicit FileLogger(const char *filename);
    ~FileLogger() override;
    bool logmsg(LoggingItem *item) override; // LoggerBase
    void reopen(void) override; // LoggerBase
    static FileLogger *create(const QString& filename, QMutex *mutex);
  private:
    std::ofstream m_ofstream; ///< Output file stream for the log file.
};

#ifndef _WIN32
/// \brief Syslog-based logger (not available in Windows)
class SyslogLogger : public LoggerBase
{
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
  public:
    JournalLogger();
    ~JournalLogger() override;
    bool logmsg(LoggingItem *item) override; // LoggerBase
    /// \brief Unused for this logger.
    void reopen(void) override { }; // LoggerBase
    static JournalLogger *create(QMutex *mutex);
};
#endif

using LoggingItemList = QList<LoggingItem *>;

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

    static void forwardMessage(LoggingItem *item);
  signals:
    void incomingSigHup(void);
  protected slots:
    static void handleSigHup(void);
};

MBASE_PUBLIC bool logForwardStart(void);
MBASE_PUBLIC void logForwardStop(void);
MBASE_PUBLIC void logForwardMessage(LoggingItem *item);


#ifndef _WIN32
MBASE_PUBLIC void logSigHup(void);
#endif

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
