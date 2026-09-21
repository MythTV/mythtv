#pragma once

#include <thread>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTextStream>
#include <QTimer>

#include "libmythbase/mthread.h"
#include "libmythbase/verbosedefs.h"

#include "posixprocess.h"
#include "process.h"

#include "config_base.h"

class TouchMonitor;

class Recorder : public QObject
{
    Q_OBJECT

    friend TouchMonitor;

  public:

    explicit Recorder(QObject* parent,
                      std::unique_ptr<ExternConfig> config,
                      QString desc);
    ~Recorder(void);

    bool processCommand(const QJsonObject& message);

    QString expandVariables(const std::string& value);

    void executeCommand(QString command,
                        QString desc,
                        bool background,
                        bool flagDamaged = false,
                        LogLevel_t logLvl = LOG_INFO);
    bool executeDetachedCommand(const QString& command,
                                const QString& desc,
                                LogLevel_t logLvl = LOG_INFO);

  private:
    void sendResponse(const QJsonObject& originalMsg,
                       const QJsonObject& responseData,
                       LogLevel_t level = LOG_INFO);

    QString getTunerValue(const QString& searchKey,
                          const QString& defaultValue = "");

    // Command handlers
    bool apiVersion(const QJsonObject& params);
    bool version(const QJsonObject& params);
    bool description(const QJsonObject& params);
    bool hasTuner(const QJsonObject& params);
    bool hasPictureAttributes(const QJsonObject& params);
    bool flowControl(const QJsonObject& params);
    bool blockSize(const QJsonObject& params);
    bool lockTimeout(const QJsonObject& params);
    bool signalStrength(const QJsonObject& params);
    bool hasLock(const QJsonObject& params);
    bool tuneChannel(const QJsonObject& params);
    bool tuneStatus(const QJsonObject& params);
    bool isOpen(const QJsonObject& params);
    bool closeRecorder(const QJsonObject& params);
    bool startStreaming(const QJsonObject& params);
    bool stopStreaming(const QJsonObject& params);
    bool xon(const QJsonObject& params);
    bool xoff(const QJsonObject& params);
    void stderrLoop(int fd);
    void streamLoop(QString command);

    bool loadChannels(const QJsonObject& params);
    void channelInfo(const QJsonObject& params,
                      std::optional<ChannelInfo> channel);
    bool firstChannel(const QJsonObject& params);
    bool nextChannel(const QJsonObject& params);

    void processVariablesInMessage(const QJsonObject& message);

    // Thread management helpers
    void StderrLine(const QString& line);

    void terminateProcesses();
    bool isProcessRunning(const QString& key);
    void initializeTouchTasks(void);

    // Member variables
    QHash<QString, Process*> m_processes;
    QHash<QString, TouchMonitor*> m_touchMonitors;
    QHash<QString, std::function<bool(const QJsonObject &)>> m_handlers;

    QMutex m_tuneMutex;
    mutable QMutex m_stderrLock;

    std::unique_ptr<ExternConfig> m_config;

    QString m_defaultDesc;
    int m_blockSize {64368};

    QString m_tunedCmd;

    bool m_hasTuner {false};
    bool m_recDirectTune {false};

    std::atomic<bool> m_isStreaming {false};
    std::atomic<bool> m_isXon {false};
    QString           m_streamState;

    std::thread m_stderrThread;
    std::thread m_streamThread;
    std::unique_ptr<PosixProcess> m_streamProcess;

    std::function<void(const QString&, const QVariantMap&)> m_mainEvent;
};
