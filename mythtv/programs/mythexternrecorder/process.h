#pragma once

#include <QObject>

#include <QDateTime>
#include <QProcess>
#include <QString>

#include <chrono>

class Process : public QObject
{
    Q_OBJECT

  public:
    enum class State
    {
        Idle,
        Starting,
        Running,
        Stopping,
        Finished
    };
    Q_ENUM(State)

      enum class Result
      {
          Unknown,
          Success,
          Failed,
          Crashed
      };
    Q_ENUM(Result)

      explicit Process(const QString& description = {},
                       QObject* parent = nullptr);
    ~Process() override = default;

    /*
     * Start a process asynchronously.
     * Returns false if the process could not be started.
     */
    bool start(const QString& command);
    static bool startDetached(const QString& command);

    /*
     * Execute a process synchronously.
     * Returns true if the process exited successfully.
     */
    bool execute(const QString& command);

    /*
     * Wait for a background process to complete.
     * timeout < 0 means wait forever.
     */
    bool wait(int timeout = -1);

    // Politely request termination.
    bool terminate(int timeout = 1000);

    // Forcefully kill the process.
    bool kill();

    /*
     * State
     */
    bool isRunning() const;
    bool isFinished() const;

    bool succeeded() const
    {
        return m_result == Result::Success;
    }

    bool failed() const
    {
        return isFinished() && !succeeded();
    }

    State state() const
    {
        return m_state;
    }

    Result result() const
    {
        return m_result;
    }

    /*
     * Process information
     */

    const QString& description() const
    {
        return m_description;
    }

    const QString& command() const
    {
        return m_command;
    }

    const QString& errorString() const
    {
        return m_errorString;
    }

    int exitCode() const
    {
        return m_exitCode;
    }

    QProcess::ExitStatus exitStatus() const
    {
        return m_exitStatus;
    }

    QProcess::ProcessError processError() const
    {
        return m_error;
    }

    qint64 processId() const;

    std::chrono::milliseconds duration() const;

  signals:
    /// Emitted whenever stdout becomes available.
    void stdoutReady(const QString& text);

    /// Emitted whenever stderr becomes available.
    void stderrReady(const QString& text);

    /// Emitted whenever the process has terminated.
    void finished(bool success, int exitCode);

  private slots:
    void processFinished(int exitCode,
                         QProcess::ExitStatus exitStatus);
    void processErrorOccurred(QProcess::ProcessError error);

    void readStandardOutput();
    void readStandardError();

  private:
    void reset();
    void setState(State state);

  private:
    QString m_description;
    QString m_command;

    QProcess m_process;

    State m_state {State::Idle};
    Result m_result {Result::Unknown};

    int m_exitCode {0};

    QProcess::ExitStatus m_exitStatus
    {QProcess::NormalExit};

    QProcess::ProcessError m_error
    {QProcess::UnknownError};

    QString m_errorString;

    QDateTime m_started;
    QDateTime m_finished;
};
