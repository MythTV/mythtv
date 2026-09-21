#pragma once

#include "libmythbase/verbosedefs.h"
#include "process.h"

#include <optional>
#include <chrono>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QJsonObject>
#include "config_base.h"

class Recorder;

class TouchMonitor : public QObject
{
    Q_OBJECT

  public:
    explicit TouchMonitor(const QString& name,
                          const TouchConfig& touch,
                          Recorder* recorderParent);
    ~TouchMonitor(void) override;

    void start();
    void stop();

  private slots:
    void triggerCommand();

  private:
    LogLevel_t parseLogLevel(const QString& levelStr);

    QString     m_name;
    TouchConfig m_config;

    Recorder*   m_recorder;

    QTimer* m_intervalTimer;
    Process* m_activeProcess  {nullptr};
    bool m_isRunning          {false};
};
