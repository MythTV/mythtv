#include <QProcess>
#include <QStandardPaths>

#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "posixprocess.h"

#include "libmythbase/mythlogging.h"

extern char** environ;

PosixProcess::~PosixProcess(void)
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

static bool matchCmdline(const std::vector<std::string>& procArgs,
                         const QString& targetCmd)
{
    if (procArgs.empty() || targetCmd.isEmpty())
        return false;

    QString target = targetCmd.trimmed();

    // Is this process running directly?
    QString directCmd;
    for (size_t idx = 0; idx < procArgs.size(); ++idx)
    {
        if (idx > 0)
            directCmd.append(' ');
        directCmd.append(QString::fromStdString(procArgs[idx]));
    }

    // If it's an exact match...
    if (directCmd.trimmed() == target)
        return true;

    // Running in a shell?
    // [0] -> "/bin/sh", [1] -> "-c", [2] -> "actual command payload"
    if (procArgs.size() >= 3)
    {
        std::string baseExec =
            std::filesystem::path(procArgs[0]).filename().string();
        std::string flag = procArgs[1];

        // Check if the binary is a common shell executing a string command
        if ((baseExec == "sh" || baseExec == "bash" || baseExec == "zsh" ||
             baseExec == "dash" || baseExec == "cmd.exe") &&
            (flag == "-c" || flag == "/c"))
        {
            if (QString::fromStdString(procArgs[2]).trimmed() == target)
                return true;
        }
    }

    return false;
}

void PosixProcess::killMatchingProcesses(const QString& targetCmd)
{
    pid_t myPid = getpid();
    pid_t parentPid = getppid();

    std::error_code ec;

    if (targetCmd.isEmpty())
        return;

    for (const auto& entry : std::filesystem::directory_iterator("/proc", ec))
    {
        if (!entry.is_directory())
            continue;

        std::string filename = entry.path().filename().string();
        if (filename.empty() ||
            !std::all_of(filename.begin(), filename.end(), ::isdigit))
        {
            continue;
        }

        pid_t pid = 0;
        try
        {
            pid = std::stoi(filename);
        }
        catch (...)
        {
            continue;
        }

        if (pid <= 1 || pid == myPid || pid == parentPid)
            continue;

        auto procArgs = getProcCmdline(pid);
        if (matchCmdline(procArgs, targetCmd))
        {
            LOG(VB_RECORD, LOG_WARNING,
                QString("Found running process matching command string (PID "
                        "%1): -> terminating")
                    .arg(pid));

            kill(pid, SIGTERM);

            // Poll process state without waitpid (since it's not our child)
            for (int idx = 0; idx < 20; ++idx)
            {
                if (kill(pid, 0) != 0 && errno == ESRCH)
                    break; // Process is officially dead
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (kill(pid, 0) == 0)
            {
                LOG(VB_RECORD, LOG_WARNING,
                    QString("PID %1 stubborn, issuing SIGKILL").arg(pid));
                kill(pid, SIGKILL);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }
}

LaunchResult PosixProcess::launch(const QString& command,
                                  const QProcessEnvironment& env,
                                  const QString& shellPath)
{
    if (m_pid > 0 || m_stdoutFd >= 0 || m_stderrFd >= 0)
        terminate();

    killMatchingProcesses(command);

    QString progPath;
    QStringList finalArgs;

    if (!shellPath.isEmpty())
    {
        progPath = shellPath;

        // Ensure the shell binary exists/is executable
        if (access(progPath.toLocal8Bit().constData(), X_OK) != 0)
        {
            QString errStr =
                QString("Specified shell '%1' not found or not executable")
                    .arg(shellPath);
            LOG(VB_RECORD, LOG_ERR, errStr);
            return {false, errStr};
        }

        finalArgs.push_back(shellPath);
        // POSIX shells use -c to execute a string string
        finalArgs.push_back("-c");
        // The entire command string is one argument
        finalArgs.push_back(command);
    }
    else
    {
        // Run without shell
        m_args = QProcess::splitCommand(command);
        if (m_args.isEmpty())
        {
            LOG(VB_RECORD, LOG_ERR,
                QString("Failed to split '%1' into args").arg(command));
            return {false,
                    QString("Failed to split '%1' into args").arg(command)};
        }

        QString execProg = m_args[0];

        if (execProg.contains('/'))
        {
            if (access(execProg.toLocal8Bit().constData(), X_OK) != 0)
            {
                QString err_str =
                    QString("Executable '%1' not found or not executable: %2")
                        .arg(execProg,
                             QString::fromLocal8Bit(strerror(errno)));
                LOG(VB_RECORD, LOG_ERR, err_str);
                return {false, err_str};
            }

            progPath = execProg;
        }
        else
        {
            progPath = QStandardPaths::findExecutable(execProg);
            if (progPath.isEmpty())
            {
                QString err_str =
                    QString("Executable '%1' not found in PATH").arg(execProg);
                LOG(VB_RECORD, LOG_ERR, err_str);
                return {false, err_str};
            }
        }

        finalArgs = m_args;
    }

    QString fullCommandLine = finalArgs.join("\" \"");
    if (!fullCommandLine.isEmpty())
        fullCommandLine = "\"" + fullCommandLine + "\"";

    LOG(VB_RECORD, LOG_INFO, QString("Launching: %1").arg(fullCommandLine));

    // Convert strings for argv
    std::vector<QByteArray> storage;
    storage.reserve(finalArgs.size());
    std::vector<char*> argv;
    argv.reserve(finalArgs.size() + 1);

    for (const QString& arg : finalArgs)
        storage.push_back(arg.toLocal8Bit());

    for (QByteArray& arg : storage)
        argv.push_back(arg.data());

    argv.push_back(nullptr);

    // convert strings for envp
    QStringList envList = env.toStringList();
    std::vector<QByteArray> env_storage;
    env_storage.reserve(envList.size());
    std::vector<char*> envp;
    envp.reserve(envList.size() + 1);

    for (const QString& env_var : envList)
        env_storage.push_back(env_var.toLocal8Bit());

    for (QByteArray& env_var : env_storage)
        envp.push_back(env_var.data());

    envp.push_back(nullptr);

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

    // Actions
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    // Map child stdout -> pipe write end
    posix_spawn_file_actions_adddup2(&actions, stdoutPipe[1], STDOUT_FILENO);

    // Map child stderr -> pipe write end
    posix_spawn_file_actions_adddup2(&actions, stderrPipe[1], STDERR_FILENO);

    // Child only writes to the pipes)
    posix_spawn_file_actions_addclose(&actions, stdoutPipe[0]);
    posix_spawn_file_actions_addclose(&actions, stderrPipe[0]);

    // Child uses the duplicated STDOUT/STDERR
    posix_spawn_file_actions_addclose(&actions, stdoutPipe[1]);
    posix_spawn_file_actions_addclose(&actions, stderrPipe[1]);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 0);

    // Spawn process
    pid_t pid;
    int spawnrc = posix_spawn(&pid, progPath.toLocal8Bit().constData(),
                              &actions, &attr, argv.data(), envp.data());

    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&actions);

    // Cleanup parent
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    if (spawnrc != 0)
    {
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        LOG(VB_RECORD, LOG_ERR,
            QString("posix_spawnp failed: %1 (%2)")
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
            m_exitCode = WEXITSTATUS(status);
            LOG(VB_RECORD, LOG_INFO,
                QString("Stream process '%1' exited with status %2")
                    .arg(m_args.join(' '))
                    .arg(WEXITSTATUS(status)));
        }
        else if (WIFSIGNALED(status))
        {
            LOG(VB_RECORD, LOG_INFO,
                QString("Stream process '%1' killed by signal %2")
                    .arg(m_args.join(' '))
                    .arg(WTERMSIG(status)));
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

    LOG(VB_RECORD, LOG_INFO, QString("terminate '%1'").arg(m_args.join(' ')));

    // A negative PID targets the process group ID
    kill(-m_pid, SIGTERM);

    int status = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);

    for (;;)
    {
        pid_t result = waitpid(m_pid, &status, WNOHANG);

        if (result == m_pid)
            break;

        if (result < 0)
            break;

        if (std::chrono::steady_clock::now() >= deadline)
        {
            // A negative PID targets the process group ID
            kill(-m_pid, SIGKILL);

            // Blocking wait on the primary child to finish total cleanup
            waitpid(m_pid, &status, 0);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    m_pid = -1;
    closeFds();
}
