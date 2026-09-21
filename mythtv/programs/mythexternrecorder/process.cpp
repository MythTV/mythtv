#include "process.h"
#include "libmythbase/mythlogging.h"

#include <QProcessEnvironment>

Process::Process(const QString& description, QObject* parent)
    : QObject(parent)
    , m_description(description)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &Process::processFinished);

    connect(&m_process, &QProcess::errorOccurred, this,
            &Process::processErrorOccurred);

    connect(&m_process, &QProcess::readyReadStandardOutput, this,
            &Process::readStandardOutput);

    connect(&m_process, &QProcess::readyReadStandardError, this,
            &Process::readStandardError);
}

void Process::reset()
{
    m_command.clear();

    m_state = State::Idle;
    m_result = Result::Unknown;

    m_exitCode = 0;
    m_exitStatus = QProcess::NormalExit;
    m_error = QProcess::UnknownError;
    m_errstr.clear();

    m_started = {};
    m_finished = {};
}

bool Process::start(const QString& command, QProcessEnvironment& env,
                    const QString& shellPath)
{
    if (isRunning())
        return false;

    reset();
    m_command = command;
    m_state = State::Starting;

    bool useShell = !shellPath.isEmpty();

    if (useShell)
    {
        m_process.setProcessEnvironment(env);
        m_process.setProgram(shellPath);
        m_process.setArguments(QStringList() << "-c" << command);
    }
    else
    {
        // Direct execution
        QStringList argv = QProcess::splitCommand(command);
        if (argv.isEmpty())
            return false;

        QString program = argv.takeFirst();
        m_process.setProgram(program);
        m_process.setArguments(argv);
    }

    m_process.start();

    if (!m_process.waitForStarted())
    {
        if (!isFinished())
        {
            m_error = m_process.error();
            m_errstr = m_process.errorString();
            m_result = Result::Failed;
            m_state = State::Finished;
        }
        return false;
    }

    m_started = QDateTime::currentDateTime();
    m_state = State::Running;

    LOG(VB_RECORD, LOG_DEBUG,
        QString("Process::Started '%1'%2")
        .arg(m_command)
        .arg(useShell ? QString(" via %1").arg(shellPath) : ""));

    return true;
}

bool Process::startDetached(const QString& command)
{
    QStringList argv = QProcess::splitCommand(command);
    QString program = argv.takeFirst();

    return QProcess::startDetached(program, argv);
}

bool Process::execute(const QString& command, QProcessEnvironment& env,
                      const QString& shellPath)
{
    if (!start(command, env, shellPath))
        return false;

    wait();

    return succeeded();
}

bool Process::wait(int timeout)
{
    if (!isRunning())
        return isFinished();

    LOG(VB_RECORD, LOG_DEBUG,
        QString("Process::wait '%1': state=%3 "
                "qprocess-state=%4")
            .arg(m_command)
            .arg(static_cast<int>(m_state))
            .arg(static_cast<int>(m_process.state())));

    return m_process.waitForFinished(timeout);
}

bool Process::terminate(int timeout)
{
    if (!isRunning())
        return true;

    m_state = State::Stopping;

    m_process.terminate();

    if (!m_process.waitForFinished(timeout))
    {
        m_process.kill();
        m_process.waitForFinished();
    }

    LOG(VB_RECORD, LOG_WARNING,
        QString("Process::terminate '%1'").arg(m_command));

    return succeeded();
}

bool Process::kill()
{
    if (!isRunning())
        return true;

    m_state = State::Stopping;

    m_process.kill();

    m_process.waitForFinished();

    return succeeded();
}

bool Process::isRunning() const
{
    return (m_state == State::Starting ||
            m_state == State::Running  ||
            m_state == State::Stopping);
}

bool Process::isFinished() const
{
    return m_state == State::Finished;
}

qint64 Process::processId() const
{
    return m_process.processId();
}

std::chrono::milliseconds Process::duration() const
{
    if (!m_started.isValid() || !m_finished.isValid())
        return std::chrono::milliseconds(0);

    return std::chrono::milliseconds(m_started.msecsTo(m_finished));
}

void Process::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_finished = QDateTime::currentDateTime();

    m_exitCode = exitCode;
    m_exitStatus = exitStatus;
    m_errstr = m_process.errorString();

    if (exitStatus == QProcess::CrashExit)
    {
        m_result = Result::Crashed;
    }
    else if (exitCode == 0)
    {
        m_result = Result::Success;
    }
    else
    {
        m_result = Result::Failed;
    }

    m_state = State::Finished;

    LOG(VB_RECORD, LOG_DEBUG, QString("Process::Finished '%1'").arg(m_command));

    // Notifies Recorder that the task is finished
    emit finished(m_result == Result::Success, exitCode);
}

void Process::processErrorOccurred(QProcess::ProcessError error)
{
    m_error = error;
    m_errstr = m_process.errorString();

    if (error == QProcess::FailedToStart)
    {
        m_finished = QDateTime::currentDateTime();
        m_result = Result::Failed;
        m_state = State::Finished;

        LOG(VB_RECORD, LOG_WARNING,
            QString("Process::Error Failed to start '%1'").arg(m_command));

        // Fail if binary didn't execute
        emit finished(false, -1);
        return;
    }

    LOG(VB_RECORD, LOG_WARNING,
        QString("Process::Error Failed '%1' '%2'")
            .arg(m_errstr)
            .arg(m_command));
}

void Process::readStandardOutput()
{
    QString text = QString::fromUtf8(m_process.readAllStandardOutput());

    if (!text.isEmpty())
        emit stdoutReady(text.trimmed());
}

void Process::readStandardError()
{
    QString text = QString::fromUtf8(m_process.readAllStandardError());

    if (!text.isEmpty())
        emit stderrReady(text.trimmed());
}
