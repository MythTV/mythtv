#include <cerrno>
#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <unistd.h>
#include <utility>
#include <vector>

// MythTV logging headers
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"

// Qt
#include <QDateTime>
#include <QJsonValue>
#include <QMutexLocker>
#include <QStringList>

#include "process.h"
#include "recorder.h"
#include "touchmonitor.h"

static QString normalizeCommand(const QString &command)
{
    return command.trimmed().toLower();
}

Recorder::Recorder(QObject *parent, std::unique_ptr<ExternConfig> config,
                   QString desc)
    : QObject(parent)
    , m_config(std::move(config))
    , m_defaultDesc(std::move(desc))
    , m_env(QProcessEnvironment::systemEnvironment())
{
    // Initialize handlers map
    m_handlers.insert(normalizeCommand("APIVersion"),
                      [this](const QJsonObject &params) -> bool
                      { return apiVersion(params); });
    m_handlers.insert(normalizeCommand("Version?"),
                      [this](const QJsonObject &params) -> bool
                      { return version(params); });
    m_handlers.insert(normalizeCommand("Description?"),
                      [this](const QJsonObject &params) -> bool
                      { return description(params); });
    m_handlers.insert(normalizeCommand("HasTuner?"),
                      [this](const QJsonObject &params) -> bool
                      { return hasTuner(params); });
    m_handlers.insert(normalizeCommand("HasPictureAttributes?"),
                      [this](const QJsonObject &params) -> bool
                      { return hasPictureAttributes(params); });
    m_handlers.insert(normalizeCommand("FlowControl?"),
                      [this](const QJsonObject &params) -> bool
                      { return flowControl(params); });
    m_handlers.insert(normalizeCommand("BlockSize"),
                      [this](const QJsonObject &params) -> bool
                      { return blockSize(params); });
    m_handlers.insert(normalizeCommand("LockTimeout?"),
                      [this](const QJsonObject &params) -> bool
                      { return lockTimeout(params); });
    m_handlers.insert(normalizeCommand("SignalStrengthPercent?"),
                      [this](const QJsonObject &params) -> bool
                      { return signalStrength(params); });
    m_handlers.insert(normalizeCommand("HasLock?"),
                      [this](const QJsonObject &params) -> bool
                      { return hasLock(params); });
    m_handlers.insert(normalizeCommand("TuneChannel"),
                      [this](const QJsonObject &params) -> bool
                      { return tuneChannel(params); });
    m_handlers.insert(normalizeCommand("TuneStatus?"),
                      [this](const QJsonObject &params) -> bool
                      { return tuneStatus(params); });
    m_handlers.insert(normalizeCommand("IsOpen?"),
                      [this](const QJsonObject &params) -> bool
                      { return isOpen(params); });
    m_handlers.insert(normalizeCommand("CloseRecorder"),
                      [this](const QJsonObject &params) -> bool
                      { return closeRecorder(params); });
    m_handlers.insert(normalizeCommand("StartStreaming"),
                      [this](const QJsonObject &params) -> bool
                      { return startStreaming(params); });
    m_handlers.insert(normalizeCommand("StopStreaming"),
                      [this](const QJsonObject &params) -> bool
                      { return stopStreaming(params); });
    m_handlers.insert(normalizeCommand("XON"),
                      [this](const QJsonObject &params) -> bool
                      { return xon(params); });
    m_handlers.insert(normalizeCommand("XOFF"),
                      [this](const QJsonObject &params) -> bool
                      { return xoff(params); });
    m_handlers.insert(normalizeCommand("LoadChannels"),
                      [this](const QJsonObject &params) -> bool
                      { return loadChannels(params); });
    m_handlers.insert(normalizeCommand("FirstChannel"),
                      [this](const QJsonObject &params) -> bool
                      { return firstChannel(params); });
    m_handlers.insert(normalizeCommand("NextChannel"),
                      [this](const QJsonObject &params) -> bool
                      { return nextChannel(params); });

    m_shell = QString::fromStdString(m_config->getValue("RECORDER",
                                                        "shell", "", false));
}

Recorder::~Recorder(void)
{
    LOG(VB_RECORD, LOG_DEBUG, "External Recorder destructor");

    if (m_isStreaming.load())
        stopStreaming(QJsonObject{});

    terminateProcesses();
}

void Recorder::terminateProcesses(void)
{
    QHash<QString, TouchMonitor *>::iterator touchIt =
        m_touchMonitors.begin();
    while (touchIt != m_touchMonitors.end())
    {
        TouchMonitor *monitor = touchIt.value();
        if (monitor)
        {
            monitor->stop();
            delete monitor;
        }
        ++touchIt;
    }
    m_touchMonitors.clear();

    // Tell every running subprocess to stop.
    // Disconnect signals first to prevent finished() slot / deleteLater race.
    for (auto it = m_processes.begin(); it != m_processes.end(); ++it)
    {
        QString key = it.key();
        Process *myProcess = it.value();

        if (myProcess)
        {
            myProcess->disconnect();

            if (myProcess->isRunning())
            {
                LOG(VB_RECORD, LOG_INFO,
                    QString("Force terminating %1 process %2")
                    .arg(key)
                    .arg(myProcess->processId()));

                // Give it 2000ms to terminate before getting aggressive.
                myProcess->terminate(2000);
            }
            delete myProcess;
        }
    }

    // Completely reset the container tracking map state
    m_processes.clear();

    m_tunedCmd.clear();
}

void Recorder::sendResponse(const QJsonObject &originalMsg,
                             const QJsonObject &respData,
                             LogLevel_t level)
{
    QJsonObject response;

    // Copy keys from incoming payload
    static const QStringList keepKeys = {"command", "serial", "value"};
    for (const QString &key : keepKeys)
    {
        if (originalMsg.contains(key))
            response.insert(key, originalMsg.value(key));
    }

    // Merge respData properties
    for (auto it = respData.begin(); it != respData.end(); ++it)
        response.insert(it.key(), it.value());

    // Can be called from different threads
    QMutexLocker locker(&m_stderrLock);

    try
    {
        if (response.value("command").toString() == "STATUS")
        {
            LOG(VB_RECORD, level,
                QString("<> %1").arg(response.value("message").toString()));
        }
        else
        {
            if (response.contains("message"))
            {
                LOG(VB_RECORD, level, QString("'%1' -> '%2'")
                    .arg(response.value("command").toString(),
                         response.value("message").toString()));
            }
            else if (response.contains("value"))
            {
                LOG(VB_RECORD, level, QString("'%1' <- '%2'")
                    .arg(response.value("command").toString(),
                         response.value("value").toVariant().toString()));
            }
            else
            {
                LOG(VB_RECORD, level, QString("> '%1'")
                    .arg(response.value("command").toString()));
            }
        }

        QJsonDocument doc(response);
        QString jsonString =
            QString::fromUtf8(doc.toJson(QJsonDocument::Compact)) + "\n";

        QTextStream stderrStrm(stderr);
        stderrStrm << jsonString;
        stderrStrm.flush();
    }
    catch (const std::exception &e)
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("Failed to send response: %1\nResponse data: %2")
            .arg(QString::fromStdString(e.what()),
                 QString::fromUtf8(QJsonDocument(response)
                                   .toJson(QJsonDocument::Compact))));
    }
}

bool Recorder::processCommand(const QJsonObject &message)
{
    const QString rawCmd =
        message.value("command").toString().trimmed();

    if (rawCmd.isEmpty())
    {
        LOG(VB_RECORD, LOG_WARNING,
            "No command parameter present inside incoming message.");
        return true;
    }

    const QString command = rawCmd.toLower();

    const auto it = m_handlers.constFind(command);
    if (it == m_handlers.constEnd())
    {
        LOG(VB_RECORD, LOG_WARNING,
            QString("Unknown backend command: %1").arg(rawCmd));

        QJsonObject response = {
            {"status", "ERR"},
            {"message", "Unknown command"}
        };

        sendResponse(message, response, LOG_WARNING);
        return true;
    }

    LogLevel_t level = LOG_DEBUG;
    if (command == "tunestatus?")
        level = LOG_TRACE;

    try
    {
        LOG(VB_RECORD, level, QString("%1 : %2")
            .arg(rawCmd,
                 QString::fromUtf8(QJsonDocument(message)
                                   .toJson(QJsonDocument::Compact))));

        return it.value()(message);
    }
    catch (const std::exception &e)
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("Standard exception caught during command execution %1: %2")
            .arg(it.key(), QString::fromStdString(e.what())));

        QJsonObject response = {
            {"status", "ERR"},
            {"message", QString::fromStdString(e.what())}
        };

        sendResponse(message, response);
    }
    catch (...)
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("Unknown unhandled critical panic running command: %1")
            .arg(it.key()));

        QJsonObject response = {
            {"status", "ERR"},
            {"message", "Unknown error"}
        };

        sendResponse(message, response, LOG_CRIT);
    }

    return false;
}

void Recorder::processVariablesInMessage(const QJsonObject &message)
{
    LOG(VB_RECORD, LOG_DEBUG,
        "Processing message variables. Message: " +
        QString::fromUtf8(QJsonDocument(message)
                          .toJson(QJsonDocument::Compact)));

    for (auto it = message.begin(); it != message.end(); ++it)
    {
        const std::string key = it.key().toStdString();

        if (key == "command" || key == "serial")
            continue;

        std::string value;

        if (it.value().isString())
            value = it.value().toString().toStdString();
        else if (it.value().isDouble())
            value = QString::number(it.value().toDouble(), 'g', 17)
                    .toStdString();
        else if (it.value().isBool())
            value = it.value().toBool() ? "true" : "false";
        else
            continue;

        m_config->updateVariable(key, value);
    }
}

QString Recorder::expandVariables(const std::string& value)
{
    return QString::fromStdString(m_config->expandVars(value));
}

QString Recorder::getTunerValue(const QString& searchKey,
                                  const QString& defaultValue)
{
    const std::string key = searchKey.toStdString();
    std::optional<std::string> value;

    if ((value = m_config->getChannelValue(key, m_shell.isEmpty())) ||
        (value = m_config->getValue("TUNER", key, m_shell.isEmpty())))
    {
        return QString::fromStdString(*value);
    }

    return defaultValue;
}


bool Recorder::apiVersion(const QJsonObject &params)
{
    // Use QJsonObject directly instead of QVariantMap
    QJsonObject response = {{"status", "OK"}, {"value", "3"}};

    sendResponse(params, response);
    return true;
}

bool Recorder::version(const QJsonObject &params)
{
    QJsonObject response = {{"status", "OK"}, {"message", MYTH_BINARY_VERSION}};

    sendResponse(params, response);
    return true;
}

bool Recorder::description(const QJsonObject &params)
{
    const std::string desc =
        m_config->getValue("RECORDER", "desc",
                           m_defaultDesc.toStdString(), true);
    QJsonObject response = {{"status", "OK"},
                            {"message", QString::fromStdString(desc)}};

    sendResponse(params, response);
    return true;
}

bool Recorder::hasTuner(const QJsonObject &params)
{
    if (!m_config->hasTable("TUNER"))
    {
        m_hasTuner = false;
        m_recDirectTune = false;
    }
    else
    {
        const auto tuneCmd  = m_config->getValue("TUNER", "command",
                                                 "", false);
        const auto tuneChan = m_config->getValue("TUNER", "channels",
                                                   "", false);

        if (!tuneCmd.empty())
        {
            // [TUNE]/command specified
            m_hasTuner = true;
            m_recDirectTune = false;
        }
        else if (!tuneChan.empty())
        {
            // [TUNE]/command not specified, but [TUNE]/channels exists
            m_hasTuner = true;
            m_recDirectTune = true;
        }
        else
        {
            // Neither [TUNE]/command nor [TUNE]/channels exists
            m_hasTuner = false;
            m_recDirectTune = false;
        }
    }

    QJsonObject response = {{"status", "OK"},
                            {"message", m_hasTuner ? "Yes" : "No"}};

    sendResponse(params, response);
    return true;
}

bool Recorder::hasPictureAttributes(const QJsonObject &params)
{
    QJsonObject response = {{"status", "OK"}, {"message", "No"}};

    sendResponse(params, response);
    return true;
}

bool Recorder::flowControl(const QJsonObject &params)
{
    QJsonObject response = {{"status", "OK"}, {"message", "XON/XOFF"}};

    sendResponse(params, response);
    return true;
}

bool Recorder::blockSize(const QJsonObject &params)
{
    if (params.contains("value"))
    {
        const QJsonValue value = params["value"];

        bool ok = false;
        int blockSize = 0;

        if (value.isString())
            blockSize = value.toString().toInt(&ok);
        else if (value.isDouble())
        {
            blockSize = value.toInt();
            ok = true;
        }

        if (ok)
            m_blockSize = std::max(blockSize, 65536);
    }

    QJsonObject response = {{"status", "OK"}, {"value", m_blockSize}};

    sendResponse(params, response);
    return true;
}

bool Recorder::lockTimeout(const QJsonObject &params)
{
    bool ok;
    QString str = getTunerValue("timeout", "3000");
    int value   = str.toInt(&ok);

    if (!ok)
    {
        LOG(VB_RECORD, LOG_WARNING, "lockTimeout logic error");
        value = 3000;
    }

    QJsonObject response = {{"status", "OK"}, {"value", value}};

    sendResponse(params, response);
    return true;
}

bool Recorder::signalStrength(const QJsonObject &params)
{
    int strength = 0;
    QString status = "OK";
    LogLevel_t level = LOG_INFO;

    if (m_recDirectTune || !m_hasTuner)
        strength = 100;
    else
    {
        QHash<QString, Process *>::const_iterator it =
            m_processes.constFind("Tune");

        if (it != m_processes.constEnd() && it.value() != nullptr)
        {
            const Process *tuneProc = it.value();

            if (tuneProc->state() == Process::State::Running ||
                tuneProc->state() == Process::State::Starting)
            {
                strength = 25;
            }
            else if (tuneProc->state() == Process::State::Finished)
            {
                if (tuneProc->succeeded())
                    strength = 100;
                else
                {
                    strength = 0;
                    status = "ERR";
                    level = LOG_ERR;
                }
            }
        }
    }

    QJsonObject response = {{"status", status}, {"value", strength}};

    sendResponse(params, response, level);
    return true;
}

bool Recorder::hasLock(const QJsonObject &params)
{
    QString message = "No";
    QString status = "OK";
    LogLevel_t level = LOG_INFO;

    if (m_recDirectTune || !m_hasTuner)
        message = "Yes";
    else
    {
        QHash<QString, Process *>::const_iterator it =
            m_processes.constFind("Tune");

        if (it != m_processes.constEnd() && it.value() != nullptr)
        {
            const Process *tuneProc = it.value();
            if (tuneProc->state() == Process::State::Finished)
            {
                if (tuneProc->succeeded())
                    message = "Yes";
                else
                {
                    message = "No";
                    status = "ERR";
                    level = LOG_ERR;
                }
            }
        }
    }

    QJsonObject response = {{"status", status}, {"message", message}};

    sendResponse(params, response, level);
    return true;
}

bool Recorder::tuneChannel(const QJsonObject &params)
{
    processVariablesInMessage(params);

    // Channel-specific URL override assignment mapping
    std::string url = getTunerValue("URL").toStdString();
    m_config->updateVariable("URL", url);

    // Some recorders tune themselves (HDHomeRun, etc.).
    if (m_recDirectTune)
    {
        QJsonObject response = {{"status", "OK"}, {"message", "Tuned"}};

        sendResponse(params, response);

        if (m_isStreaming)
        {
            QString cmd = getTunerValue("newepisode");
            executeCommand(cmd, "newepisode", true, false, LOG_DEBUG);
        }

        return true;
    }

    // Obtain the tune command from either the channel specific block
    // or the global default
    QString tuneCmd = getTunerValue("command");

    if (tuneCmd.isEmpty())
    {
        QJsonObject response = {{"status", "ERR"},
                                {"message", "No tune command provided"}};

        sendResponse(params, response);
        return true;
    }

    // Evaluate if we are already tuned to this specific channel.
    // Check if the "Tune" process key exists within Process map container.
    QHash<QString, Process *>::const_iterator it =
        m_processes.constFind("Tune");

    if (it != m_processes.constEnd())
    {
        const Process *tuneProc = it.value();
        // Check if the existing tuner finished running
        if (tuneProc)
        {
            if (tuneProc->succeeded() && tuneCmd == m_tunedCmd)
            {
                LOG(VB_RECORD, LOG_INFO,
                    QString("Already tuned to `(%1)`").arg(tuneCmd));

                QJsonObject response = {{"status", "OK"}, {"message", "Tuned"}};

                sendResponse(params, response, LOG_INFO);

                QString cmd = getTunerValue("newepisode");
                executeCommand(cmd, "newepisode", true, false, LOG_DEBUG);

                return true;
            }
            if (tuneProc->isRunning() && tuneCmd == m_tunedCmd)
            {
                LOG(VB_CHANNEL, LOG_INFO,
                    QString("Already tuning to `%1`").arg(tuneCmd));

                QJsonObject response = {{"status", "OK"},
                                        {"message", "InProgress"}};

                sendResponse(params, response, LOG_DEBUG);
                return true;
            }
        }
    }

    LOG(VB_RECORD, LOG_INFO,
        QString("Tuning with %1").arg(tuneCmd));

    m_tunedCmd = tuneCmd;
    executeCommand(tuneCmd, "Tune", true);

    QJsonObject response = {
        {"status", "OK"},
        {"message", "InProgress"}
    };

    sendResponse(params, response, LOG_DEBUG);
    return true;
}

bool Recorder::tuneStatus(const QJsonObject &params)
{
    QString message;
    QString status = "OK";
    LogLevel_t level = LOG_INFO;

    // Some recorders tune themselves (HDHomeRun, etc.).
    if (m_recDirectTune)
        message = "Tuned";
    else
    {
        // Find Tune process.
        QHash<QString, Process *>::const_iterator it =
            m_processes.constFind("Tune");

        if (it == m_processes.constEnd() || it.value() == nullptr)
            message = "Idle";
        else
        {
            const Process *tuneProc = it.value();

            switch (tuneProc->state())
            {
                case Process::State::Starting:
                case Process::State::Running:
                {
                    message = "InProgress";
                    level = LOG_DEBUG;
                    break;
                }
                case Process::State::Finished:
                {
                    // Check if it finished successfully or failed/crashed
                    if (tuneProc->succeeded())
                        message = "Tuned";
                    else
                    {
                        message = "Failed";
                        status = "ERR";
                        level = LOG_ERR;
                    }
                    break;
                }
                case Process::State::Stopping:
                case Process::State::Idle:
                default:
                {
                    message = "Idle";
                    break;
                }
            }
        }
    }

    QJsonObject response = {{"status", status}, {"message", message}};

    sendResponse(params, response, level);
    return true;
}

bool Recorder::isOpen(const QJsonObject &params)
{
    QString message = m_isStreaming.load() ? "Open" : "No";

    QJsonObject response = {{"status", "OK"}, {"message", message}};

    sendResponse(params, response);
    return true;
}

bool Recorder::closeRecorder(const QJsonObject &params)
{
    LOG(VB_RECORD, LOG_DEBUG,
        "CloseRecorder command received from backend. "
        "Stopping all operations.");

    // Stop streaming if active
    if (m_isStreaming)
        stopStreaming(params);

    terminateProcesses();

    QJsonObject response = {{"status", "OK"}, {"message", "RecorderClosed"}};

    sendResponse(params, response);
    return true;
}

bool Recorder::startStreaming(const QJsonObject &params)
{
    if (m_isStreaming.load())
    {
        LOG(VB_RECORD, LOG_INFO, "Already streaming");

        QJsonObject response = {{"status", "WARN"},
                                {"message", "Already streaming"}};

        sendResponse(params, response);
        return true;
    }

    // Make sure there is not a dangling thread
    if (m_streamThread.joinable())
    {
        LOG(VB_RECORD, LOG_WARNING,
            "Joining previous stream thread before starting a new one");
        m_streamThread.join();
    }

    // Channel-specific URL override assignment mapping
    std::string url = getTunerValue("URL", "").toStdString();
    m_config->updateVariable("URL", url);

    std::string profile;
    if (params.contains("profile"))
    {
        QString p = params.value("profile").toString().toLower();
        p.remove(QRegularExpression("\\s"));
        profile = p.toStdString();
    }

    if (params.contains("reason"))
    {
        m_streamState = params.value("reason").toString().toLower();
        LOG(VB_RECORD, LOG_INFO, QString("StartStreaming: %1")
            .arg(m_streamState));
    }

    std::string key = profile.empty() ? "command" : "command_" + profile;
    std::string streamCmd = m_config->getValue("RECORDER", key,
                                               "", m_shell.isEmpty());
    if (streamCmd.empty() && !profile.empty())
    {
        LOG(VB_RECORD, LOG_INFO,
            QString("[RECORDER]/%1 not provided, Using [RECORDER]/command")
            .arg(QString::fromStdString(key)));
        key = "command";
        streamCmd = m_config->getValue("RECORDER", key, "", m_shell.isEmpty());
    }

    if (streamCmd.empty())
    {
        QString message = QString("No [RECORDER]/%1 specified")
                          .arg(QString::fromStdString(key));
        LOG(VB_RECORD, LOG_WARNING, message);

        QJsonObject response = {{"status", "ERR"}, {"message", message}};

        sendResponse(params, response);
        return true;
    }

    LOG(VB_RECORD, LOG_INFO,
        QString("[RECORDER]/%1 : %2")
        .arg(QString::fromStdString(key), QString::fromStdString(streamCmd)));

    QJsonObject response = {{"status", "OK"}, {"message", "Streaming started"}};
    sendResponse(params, response);

    m_isStreaming.store(true);

    m_streamProcess = std::make_unique<PosixProcess>();
    m_streamThread = std::thread(&Recorder::streamLoop, this,
                                  QString::fromStdString(streamCmd));

    pthread_setname_np(m_streamThread.native_handle(), "stream");

    return true;
}

bool Recorder::stopStreaming(const QJsonObject &params)
{
    bool wasStreaming = m_isStreaming.exchange(false);

    // streamLoop may be blocked in read(stdout_input_fd).
    if (m_streamProcess && m_streamProcess->pid() > 0)
        m_streamProcess->terminate();

    if (m_streamThread.joinable())
        m_streamThread.join();

    if (!wasStreaming)
    {
        LOG(VB_RECORD, LOG_WARNING,
            "Received StopStreaming, but engine was already idle.");

        QJsonObject response = {
            {"status", "ERR"},
            {"message", "Not streaming"}
        };

        sendResponse(params, response);
        return true;
    }

    if (params.contains("reason"))
    {
        m_streamState =
            params.value("reason").toString().toLower();

        LOG(VB_RECORD, LOG_INFO,
            QString("StopStreaming: %1").arg(m_streamState));
    }

    QJsonObject response = {
        {"status", "OK"},
        {"message", "Streaming stopped"}
    };

    sendResponse(params, response);

    if (m_streamState == "finished")
    {
        QString cmd = getTunerValue("recfinished");
        executeDetachedCommand(cmd, "recfinished", LOG_DEBUG);
    }

    return true;
}

bool Recorder::xon(const QJsonObject &params)
{
    QJsonObject response = {{"status", "OK"}, {"message", "Data flowing"}};

    sendResponse(params, response);

    if (m_streamState == "recording")
    {
        QString cmd = getTunerValue("recstarting");
        executeCommand(cmd, "recstarting", false, false, LOG_DEBUG);

        initializeTouchTasks();
    }

    m_isXon.store(true);

    if (m_streamState == "recording")
    {
        QString cmd = getTunerValue("recstarted");
        executeCommand(cmd, "recstarted", true, false, LOG_DEBUG);
    }

    LOG(VB_RECORD, LOG_INFO, "Data is now flowing to Myth.");

    return true;
}

bool Recorder::xoff(const QJsonObject &params)
{
    m_isXon.store(false);

    QJsonObject response = {{"status", "OK"}, {"message", "Data paused"}};

    sendResponse(params, response, LOG_DEBUG);

    m_isXon.store(false);

    return true;
}

void Recorder::channelInfo(const QJsonObject &params,
                            std::optional<ChannelInfo> channel)
{
    if (!channel)
    {
        QJsonObject response = {{"status", "WARN"}, {"message", "DONE"}};

        sendResponse(params, response);
        return;
    }

    const std::string message = channel->number + "," + channel->name + "," +
                                channel->callsign + "," + channel->xmltvid +
                                "," + channel->icon;

    QJsonObject response = {{"status", "OK"},
                            {"message", QString::fromStdString(message)}};

    sendResponse(params, response);
}

bool Recorder::loadChannels(const QJsonObject &params)
{
    std::string scanCmd = m_config->getValue("SCANNER", "command",
                                             "", m_shell.isEmpty());
    if (!scanCmd.empty())
    {
        LOG(VB_CHANNEL, LOG_INFO, "Populate channels");
        executeCommand(QString::fromStdString(scanCmd), "loadChannels", false);
    }

    LOG(VB_CHANNEL, LOG_INFO, "Reading channels");
    m_config->loadChannels();

    QJsonObject response = {
        {"status", "OK"},
        {"value", static_cast<int>(m_config->channelCount())}};

    sendResponse(params, response);
    return true;
}

bool Recorder::firstChannel(const QJsonObject &params)
{
    channelInfo(params, m_config->firstChannel());
    return true;
}

bool Recorder::nextChannel(const QJsonObject &params)
{
    channelInfo(params, m_config->nextChannel());
    return true;
}

void Recorder::variablesToEnv(void)
{
    const ExternConfig::VarContainer& vars = m_config->allVariables();

    for (const auto& [key, value] : vars) {
        m_env.insert(
            QString::fromStdString(key),
            QString::fromStdString(value)
        );
    }
}

void Recorder::executeCommand(QString command, QString desc, bool background,
                              bool flag_damaged, LogLevel_t logLvl)
{
    command = command.trimmed();
    if (command.isEmpty())
    {
        LOG(VB_RECORD, logLvl, QString("%1 not specified").arg(desc));
        return;
    }

    // Terminate any duplicate process already assigned to this
    // description key
    QHash<QString, Process *>::iterator it = m_processes.find(desc);
    if (it != m_processes.end())
    {
        Process *active_proc = it.value();
        if (active_proc)
        {
            active_proc->disconnect();
            if (active_proc->isRunning())
            {
                LOG(VB_RECORD, logLvl,
                    QString("Force terminating duplicate process: %1")
                    .arg(desc));
                active_proc->terminate(1000);
            }
            active_proc->deleteLater();
        }
        m_processes.erase(it);
    }

    if (command.endsWith('&'))
    {
        command = command.left(command.length() - 1).trimmed();
        background = true;
    }

    Process *myProcess = new Process(desc, this);

    // Capture standard error and output logs
    connect(myProcess, &Process::stdoutReady, this,
            [desc, logLvl](const QString &text)
            { LOG(VB_RECORD, logLvl, QString("%1| %2").arg(desc, text)); });

    connect(myProcess, &Process::stderrReady, this,
            [desc, logLvl](const QString &text)
            { LOG(VB_RECORD, logLvl, QString("%1| %2").arg(desc, text)); });

    connect(myProcess, &Process::finished, this,
            [this, desc, flag_damaged, logLvl, command, myProcess]
            (bool success, int exitCode)
            {
                if (success)
                {
                    LOG(VB_RECORD, logLvl,
                        QString("`%1` completed successfully").arg(command));
                }
                else
                {
                    LOG(VB_RECORD, LOG_WARNING,
                        QString("`%1` completed with code %2")
                        .arg(command).arg(exitCode));
                    if (flag_damaged)
                    {
                        QJsonObject message = {{"command", "STATUS"}};
                        QJsonObject response = {
                            {"status", "DAMAGED"},
                            {"message", QString("DAMAGED: '%1' failed (%2)")
                             .arg(command)
                             .arg(exitCode)}
                        };

                        sendResponse(message, response, LOG_WARNING);
                    }
                }

                // Preserve tuner state
                if (desc != "Tune")
                {
                    if (m_processes.value(desc) == myProcess)
                        m_processes.remove(desc);

                    myProcess->deleteLater();
                }
            });

    // Track the running process
    m_processes.insert(desc, myProcess);

    if (!m_shell.isEmpty())
        variablesToEnv();

    if (background)
    {
        if (!myProcess->start(command, m_env, m_shell))
        {
            LOG(VB_RECORD, LOG_ERR,
                QString("Error starting background %1 command").arg(desc));
            m_processes.remove(desc);
            myProcess->deleteLater();
            return;
        }

        LOG(VB_RECORD, logLvl,
            QString("Started background %1 command (%2): %3")
            .arg(desc)
            .arg(myProcess->processId())
            .arg(command));
    }
    else
    {
        // Blocks the loop until process finishes
        LOG(VB_RECORD, logLvl,
            QString("Executing foreground command: %1").arg(command));

        bool result = myProcess->execute(command, m_env);

        LOG(VB_RECORD, LOG_WARNING,
            QString("Foreground execute returned: %1 succeeded=%2")
            .arg(result)
            .arg(myProcess->succeeded()));
    }
}

bool Recorder::executeDetachedCommand(const QString& command,
                                      const QString& desc, LogLevel_t logLvl)
{
    QString cmd = command.trimmed();
    if (cmd.isEmpty())
    {
        LOG(VB_RECORD, logLvl, QString("%1 not specified").arg(desc));
        return true;
    }

    LOG(VB_RECORD, logLvl,
        QString("Starting detached %1 command: %2")
        .arg(desc, cmd));

    if (!Process::startDetached(cmd))
    {
        LOG(VB_RECORD, LOG_WARNING,
            QString("Unable to start detached %1 command: %2")
            .arg(desc, command));
        return false;
    }

    return true;
}

void Recorder::StderrLine(const QString &line)
{
    struct LevelInfo
    {
        QStringView prefix;
        QString status;
        LogLevel_t level;
        bool send_status;
    };

    static const LevelInfo levels[] = {
        {u"crit",   "CRIT",  LOG_CRIT,    true},
        {u"err",    "ERR",   LOG_ERR,     true},
        {u"warn",   "WARN",  LOG_WARNING, true},
        {u"damage", "",      LOG_WARNING, true},
        {u"info",   "INFO",  LOG_INFO,    false},
        {u"debug",  "DEBUG", LOG_DEBUG,   false},
        {u"trace",  "TRACE", LOG_TRACE,   false},
    };

    const QString lower = line.trimmed().toLower();

    QString status = "INFO";
    LogLevel_t level = LOG_INFO;
    QString message = line;
    bool send;

    for (const auto& info : levels)
    {
        if (!lower.startsWith(info.prefix))
            continue;

        const int colon = line.indexOf(':');

        QString prefix;
        if (colon >= 0)
        {
            prefix = line.left(colon);
            message = line.mid(colon + 1);

            if (message.isEmpty())
                message = prefix;
        }
        else
        {
            prefix = line;
            message = line;
        }

        level = info.level;
        send  = info.send_status;

        if (info.prefix == u"damage" ||
            message.trimmed().toLower().startsWith(u"damage"))
        {
            status = m_isXon.load() ? "DAMAGED" : "LOST";
            send = true;
        }
        else
            status = info.status;

        break;
    }

    if (send)
    {
        QJsonObject context = {{"command", "STATUS"}};
        QJsonObject response = {
            {"status", status},
            {"message", message.remove(QRegularExpression(R"(\s+$)"))}
        };

        sendResponse(context, response, level);
    }
    else
    {
        LOG(VB_RECORD, level, QString("<> %1")
            .arg(message.remove(QRegularExpression(R"(\s+$)"))));
    }
}

void Recorder::stderrLoop(int fd)
{
    FILE *stream = fdopen(fd, "r");

    if (!stream)
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("Unable to open stderr fd %1: %2")
            .arg(fd)
            .arg(strerror(errno)));
        return;
    }

    char *line = nullptr;
    size_t size = 0;

    while (m_isStreaming.load())
    {
        ssize_t length = getline(&line, &size, stream);

        if (length < 0)
        {
            if (!m_isStreaming.load())
                break;

            if (feof(stream))
                break;

            if (ferror(stream))
            {
                if (errno == EINTR)
                {
                    clearerr(stream);
                    continue;
                }


                LOG(VB_RECORD, LOG_ERR,
                    QString("getline error on fd %1: %2")
                    .arg(fd)
                    .arg(strerror(errno)));
                break;
            }
        }

        QString message =
            QString::fromUtf8(line, static_cast<int>(length));

        if (!message.trimmed().isEmpty())
            StderrLine(message);
    }

    if (line)
        free(line);

    fclose(stream);
}

void Recorder::streamLoop(QString command)
{
    if (!m_shell.isEmpty())
        variablesToEnv();

    // Launch POSIX child process
    auto res = m_streamProcess->launch(command, m_env, m_shell);
    if (!res.ok)
    {
        LOG(VB_RECORD, LOG_ERR,
            QString("Unable to start stream process: %1").arg(res.error));
        m_isStreaming.store(false);

        QJsonObject context = {{"command", "STATUS"}};
        QJsonObject response = {
            {"status", "ERR"},
            {"message",
             QString("Failed to start stream process: %1").arg(res.error)}};
        sendResponse(context, response, LOG_ERR);
        return;
    }

    const int stderrFd = m_streamProcess->takeStderrFd();
    m_stderrThread = std::thread(&Recorder::stderrLoop, this, stderrFd);


#if defined(Q_OS_LINUX)
    pthread_setname_np(m_stderrThread.native_handle(), "stream-stderr");
#endif

    std::vector<uint8_t> buffer(static_cast<size_t>(m_blockSize));
    int stdoutInputFd = m_streamProcess->stdoutFd();

    while (m_isStreaming.load())
    {
        ssize_t bytes = ::read(stdoutInputFd, buffer.data(), buffer.size());

        if (bytes < 0)
        {
            if (errno == EINTR)
                continue;

            LOG(VB_RECORD, LOG_ERR,
                QString("read() failed: %1").arg(strerror(errno)));
            break;
        }

        if (bytes == 0)
        {
            LOG(VB_RECORD, LOG_INFO, "Stream process closed stdout (EOF)");
            break;
        }

        if (!m_isXon.load())
            continue;

        ssize_t offset = 0;
        while (offset < bytes)
        {
            ssize_t written = ::write(STDOUT_FILENO, buffer.data() + offset,
                                      static_cast<size_t>(bytes - offset));

            if (written < 0 && errno == EINTR)
                continue;

            if (written <= 0)
            {
                m_isStreaming.store(false);
                LOG(VB_RECORD, LOG_WARNING,
                    QString("Failed to write to stdout: %1")
                    .arg(strerror(errno)));
                break;
            }

            offset += written;
        }
    }

    bool wasStreaming = m_isStreaming.load();

    m_isStreaming.store(false);

    bool processStillRunning = m_streamProcess->isRunning();

    if (wasStreaming)
    {
        if (!processStillRunning)
        {
            LOG(VB_RECORD, LOG_ERR, "Stream process died unexpectedly.");

            QJsonObject context = {{"command", "STATUS"}};
            QJsonObject response = {
                {"status", "ERR"},
                {"message", "Stream process crashed or exited unexpectedly."}};
            sendResponse(context, response, LOG_ERR);
        }
        else
        {
            LOG(VB_RECORD, LOG_ERR,
             "Stream process is still running, but stdout closed prematurely.");
        }
    }

    // Clean up the process group if it's somehow still alive
    if (m_streamProcess && m_streamProcess->pid() > 0)
        m_streamProcess->terminate();

    if (m_stderrThread.joinable())
        m_stderrThread.join();

    LOG(VB_RECORD, LOG_DEBUG,
        "POSIX stream pipeline and tracking threads successfully torn down.");
}

void Recorder::initializeTouchTasks(void)
{
    for (const auto &[name, touch] : m_config->touchConfigs())
    {
        // name: "internet", "keepalive", etc.
        // touch.delay
        // touch.frequency
        // touch.command
        // touch.damaged_on_failure
        // touch.log_level
        QString taskName = QString::fromStdString(name);

        LOG(VB_RECORD, LOG_INFO,
            QString("Registering touch monitor task: %1")
            .arg(taskName));
        TouchMonitor *monitor = new TouchMonitor(taskName, touch, this);
        m_touchMonitors.insert(taskName, monitor);

        monitor->start();
    }
}
