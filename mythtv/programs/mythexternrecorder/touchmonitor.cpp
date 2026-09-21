// MythTV logging headers
#include "libmythbase/mythlogging.h"
#include "libmythbase/verbosedefs.h"

#include "touchmonitor.h"
#include "recorder.h"

#include <QStringList>
#include <QDebug>


TouchMonitor::TouchMonitor(const QString& name,
                           const TouchConfig& touch,
                           Recorder* recorderParent)
    : QObject(recorderParent)
    , m_name(name)
    , m_config(touch)
    , m_recorder(recorderParent)
    , m_intervalTimer(new QTimer(this))
{
    // Configure recurring execution timing slot
    connect(m_intervalTimer, &QTimer::timeout,
            this, &TouchMonitor::triggerCommand);
}

TouchMonitor::~TouchMonitor(void)
{
    TouchMonitor::stop();
}

LogLevel_t TouchMonitor::parseLogLevel(const QString& levelStr)
{
    QString upper = levelStr.toUpper().trimmed();
    if (upper == "CRIT")
    {
        return LOG_CRIT;
    }
    if (upper == "ERROR" || upper == "ERR")
    {
        return LOG_ERR;
    }
    if (upper == "WARN" || upper == "WARNING")
    {
        return LOG_WARNING;
    }
    if (upper == "INFO")
    {
        return LOG_INFO;
    }
    return LOG_DEBUG; // Default fallback level
}

static QString toQString(const TouchConfig& config)
{
    const auto formatSeconds = [](const auto& value) -> QString
    {
        if (!value)
            return "none";

        return QString::fromStdString(
            std::format("{}s", value->count()));
    };

    return QString("delay=%1, frequency=%2, command=\"%3\", "
                   "damaged_on_failure=%4, log_level=\"%5\"")
        .arg(formatSeconds(config.delay))
        .arg(formatSeconds(config.frequency))
        .arg(QString::fromStdString(config.command))
        .arg(config.damagedOnFailure ? "true" : "false")
        .arg(QString::fromStdString(config.logLevel));
}

void TouchMonitor::start(void)
{
    if (m_isRunning)
        return;

    LOG(VB_RECORD, LOG_INFO, QString("TouchMonitor: %1 : %2")
        .arg(m_name, toQString(m_config)));

    m_isRunning = true;

    if (m_config.delay)
        m_intervalTimer->start(*m_config.delay);
    else
        triggerCommand();
}

void TouchMonitor::stop(void)
{
    m_isRunning = false;
    m_intervalTimer->stop();

    if (m_activeProcess)
    {
        if (m_activeProcess->isRunning())
        {
            m_activeProcess->kill(); // Clear hanging scripts
        }
        m_activeProcess->deleteLater();
        m_activeProcess = nullptr;
    }
}

void TouchMonitor::triggerCommand(void)
{
    if (!m_isRunning)
        return;

    QString cmd = m_recorder->expandVariables(m_config.command);
    m_recorder->executeCommand
        (cmd,
         QString("Touch:%1").arg(m_name),
         true,
         m_config.damagedOnFailure,
         parseLogLevel(QString::fromStdString(m_config.logLevel)));

    if (m_config.frequency)
        m_intervalTimer->start(*m_config.frequency);
    else
    {
        m_intervalTimer->stop();
        m_isRunning = false;
    }
}
