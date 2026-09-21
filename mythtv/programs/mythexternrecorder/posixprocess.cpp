#include <QProcess>
#include <QStandardPaths>
#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>

#include "posixprocess.h"

#include "libmythbase/mythlogging.h"

extern char **environ;

PosixProcess::~PosixProcess()
{
    this->terminate();
}

void PosixProcess::closeFds(void)
{
    if (m_stdoutFd >= 0)
    {
        close(m_stdoutFd);
        m_stdoutFd = -1;
    }

    if (m_stderrFd >= 0)
    {
        close(m_stderrFd);
        m_stderrFd = -1;
    }
}

static std::vector<std::string> getProcCmdline(pid_t pid)
{
    std::vector<std::string> args;
    std::string path = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return args;

    std::string arg;
    while (std::getline(file, arg, '\0'))
    {
        if (!arg.empty())
            args.push_back(arg);
    }
    return args;
}

static bool matchCmdline(const std::vector<std::string>& procArgs, const QStringList& targetArgs)
{
    if (procArgs.empty() || targetArgs.isEmpty())
        return false;

    if (procArgs.size() != static_cast<size_t>(targetArgs.size()))
        return false;

    for (size_t i = 1; i < procArgs.size(); ++i)
    {
        if (procArgs[i] != targetArgs[static_cast<qsizetype>(i)].toStdString())
            return false;
    }

    std::string procExec = procArgs[0];
    std::string targetExec = targetArgs[0].toStdString();

    if (procExec == targetExec)
        return true;

    auto procBase = std::filesystem::path(procExec).filename().string();
    auto targetBase = std::filesystem::path(targetExec).filename().string();
    return procBase == targetBase;
}

void PosixProcess::killMatchingProcesses(const QStringList& args)
{
    pid_t myPid = getpid();
    std::error_code ec;

    for (const auto& entry : std::filesystem::directory_iterator("/proc", ec))
    {
        if (!entry.is_directory())
            continue;

        std::string filename = entry.path().filename().string();
        if (filename.empty() || !std::all_of(filename.begin(), filename.end(), ::isdigit))
            continue;

        pid_t pid = 0;
        try
        {
            pid = std::stoi(filename);
        }
        catch (...)
        {
            continue;
        }

        if (pid <= 1 || pid == myPid)
            continue;

        auto procArgs = getProcCmdline(pid);
        if (matchCmdline(procArgs, args))
        {
            LOG(VB_RECORD, LOG_WARNING,
                QString("Found running process with identical arguments (PID %1): %2 -> terminating")
                .arg(pid).arg(args.join(' ')));

            kill(pid, SIGTERM);

            for (int i = 0; i < 20; ++i)
            {
                int status = 0;
                pid_t wr = waitpid(pid, &status, WNOHANG);
                if (wr == pid || kill(pid, 0) != 0)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (kill(pid, 0) == 0)
            {
                LOG(VB_RECORD, LOG_WARNING,
                    QString("Process PID %1 did not exit after SIGTERM, killing with SIGKILL")
                    .arg(pid));
                kill(pid, SIGKILL);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                int status = 0;
                waitpid(pid, &status, WNOHANG);
            }
        }
    }
}

LaunchResult PosixProcess::launch(const QString& command)
{
    if (m_pid > 0 || m_stdoutFd >= 0 || m_stderrFd >= 0)
    {
        terminate();
    }

    m_args = QProcess::splitCommand(command);

    if (m_args.isEmpty())
    {
        LOG(VB_RECORD, LOG_ERR, QString("Failed to split '%1' into args")
            .arg(command));
        return {false, QString("Failed to split '%1' into args").arg(command)};
    }

    killMatchingProcesses(m_args);

    LOG(VB_RECORD, LOG_INFO, QString("Launching %1").arg(m_args.join(' ')));

    QString execProgram = m_args[0];
    if (execProgram.contains('/'))
    {
        if (access(execProgram.toLocal8Bit().constData(), X_OK) != 0)
        {
            QString errStr = QString("Executable '%1' not found or not executable: %2")
                             .arg(execProgram, QString::fromLocal8Bit(strerror(errno)));
            LOG(VB_RECORD, LOG_ERR, errStr);
            return {false, errStr};
        }
    }
    else
    {
        QString foundPath = QStandardPaths::findExecutable(execProgram);
        if (foundPath.isEmpty())
        {
            QString errStr = QString("Executable '%1' not found in PATH").arg(execProgram);
            LOG(VB_RECORD, LOG_ERR, errStr);
            return {false, errStr};
        }
    }

    int stdoutPipe[2];
    int stderrPipe[2];

    if (pipe2(stdoutPipe, O_CLOEXEC) < 0)
    {
        QString errStr = QString("Failed to create stdout pipe: %1")
                         .arg(QString::fromLocal8Bit(strerror(errno)));
        LOG(VB_RECORD, LOG_ERR, errStr);
        return {false, errStr};
    }

    if (pipe2(stderrPipe, O_CLOEXEC) < 0)
    {
        QString errStr = QString("Failed to create stderr pipe: %1")
                         .arg(QString::fromLocal8Bit(strerror(errno)));
        LOG(VB_RECORD, LOG_ERR, errStr);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return {false, errStr};
    }

    std::vector<QByteArray> storage;
    storage.reserve(m_args.size());

    std::vector<char*> argv;
    for (const QString& arg : m_args)
        storage.push_back(arg.toLocal8Bit());

    for (QByteArray& arg : storage)
        argv.push_back(arg.data());

    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    // Child stdout -> pipe write end
    posix_spawn_file_actions_adddup2(&actions, stdoutPipe[1], STDOUT_FILENO);

    // Child stderr -> pipe write end
    posix_spawn_file_actions_adddup2(&actions, stderrPipe[1], STDERR_FILENO);

    // Child does not need pipe read ends open
    posix_spawn_file_actions_addclose(&actions, stdoutPipe[0]);
    posix_spawn_file_actions_addclose(&actions, stderrPipe[0]);
    posix_spawn_file_actions_addclose(&actions, stdoutPipe[1]);
    posix_spawn_file_actions_addclose(&actions, stderrPipe[1]);

    pid_t pid;
    int spawnrc = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(),
                              environ);

    posix_spawn_file_actions_destroy(&actions);

    // Parent does not write to these pipes
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    if (spawnrc != 0)
    {
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        LOG(VB_RECORD, LOG_ERR, QString("posix_spawnp failed: %1 (%2)")
            .arg(command, QString::fromLocal8Bit(strerror(spawnrc))));
        return {false, QString::fromLocal8Bit(strerror(spawnrc))};
    }

    m_pid = pid;
    m_stdoutFd = stdoutPipe[0];
    m_stderrFd = stderrPipe[0];

    return {true, {}};
}

bool PosixProcess::isRunning(void)
{
    if (m_pid <= 0)
        return false;

    int status = 0;
    pid_t result = waitpid(m_pid, &status, WNOHANG);
    if (result == 0)
        return true; // Still running

    if (result == m_pid)
    {
        // Process has terminated
        if (WIFEXITED(status))
        {
            LOG(VB_RECORD, LOG_INFO,
                QString("Stream process '%1' exited with status %2")
                .arg(m_args.join(' ')).arg(WEXITSTATUS(status)));
        }
        else if (WIFSIGNALED(status))
        {
            LOG(VB_RECORD, LOG_INFO,
                QString("Stream process '%1' killed by signal %2")
                .arg(m_args.join(' ')).arg(WTERMSIG(status)));
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO,
                QString("Stream process '%1' died for unknown reason")
                .arg(m_args.join(' ')));
        }
        m_pid = -1;
        return false;
    }

    if (result < 0 && errno == ECHILD)
    {
        m_pid = -1;
        return false;
    }

    return false;
}

void PosixProcess::terminate(void)
{
    std::scoped_lock lock(m_mutex);

    if (m_pid <= 0)
    {
        closeFds();
        return;
    }

    LOG(VB_RECORD, LOG_INFO,
        QString("terminate '%1'").arg(m_args.join(' ')));

    kill(m_pid, SIGTERM);

    int status = 0;

    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::seconds(1);

    for (;;)
    {
        pid_t result = waitpid(m_pid, &status, WNOHANG);

        if (result == m_pid)
            break;

        if (result < 0)
            break;

        if (std::chrono::steady_clock::now() >= deadline)
        {
            kill(m_pid, SIGKILL);
            waitpid(m_pid, &status, 0);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    m_pid = -1;
    closeFds();
}


bool PosixProcess::checkStatus(void)
{
    if (m_pid <= 0)
        return false;

    int status = 0;
    pid_t result = waitpid(m_pid, &status, 0);

    if (result == m_pid)
    {
        m_pid = -1;
        if (WIFEXITED(status))
        {
            int exit_code = WEXITSTATUS(status);
            LOG(VB_RECORD, LOG_INFO,
                QString("Stream process exited with status %1")
                .arg(exit_code));
            return (exit_code == 0);
        }
        if (WIFSIGNALED(status))
        {
            LOG(VB_RECORD, LOG_INFO,
                QString("Stream process killed by signal %1")
                .arg(WTERMSIG(status)));
            return false;
        }
        LOG(VB_RECORD, LOG_INFO,
            "Stream process died for unknown reason");
        return false;
    }

    if (result < 0 && errno == ECHILD)
    {
        m_pid = -1;
        return false;
    }

    LOG(VB_RECORD, LOG_WARNING,
        QString("Expected pid %1 but got %2").arg(m_pid).arg(result));
    return false;
}
