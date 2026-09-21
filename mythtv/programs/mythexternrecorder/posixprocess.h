#pragma once

#include <QString>
#include <QStringList>

#include <sys/types.h>
#include <mutex>

struct LaunchResult
{
    bool ok;
    QString error;
};

class PosixProcess
{
  public:
    PosixProcess(void) = default;
    PosixProcess(const PosixProcess&) = delete;
    PosixProcess& operator=(const PosixProcess&) = delete;
    ~PosixProcess(void);

    LaunchResult launch(const QString& command,
                        const QProcessEnvironment& env,
                        const QString& shell_path = "");

    bool isRunning(void);
    void terminate(void);

    int   exitCode(void) const { return m_exitCode; }
    bool  valid() const        { return m_pid > 0; }
    pid_t pid(void) const      { return m_pid; }
    bool  hasStdout() const    { return m_stdoutFd >= 0; }
    bool  hasStderr() const    { return m_stderrFd >= 0; }
    int   stdoutFd(void) const { return m_stdoutFd; }

    int takeStderrFd(void)
    {
        int fd = m_stderrFd;
        m_stderrFd = -1;
        return fd;
    }

  private:
    void closeFds(void);
    void killMatchingProcesses(const QString& comamnd);

    pid_t m_pid {-1};
    int   m_exitCode {0};
    int   m_stdoutFd {-1};
    int   m_stderrFd {-1};
    QStringList m_args;

    mutable std::mutex m_mutex;
};
