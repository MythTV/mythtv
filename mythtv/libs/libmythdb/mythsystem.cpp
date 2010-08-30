
// Own header
#include "mythsystem.h"

// compat header
#include "compat.h"

// C++/C headers
#include <cerrno>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

// QT headers
#include <QCoreApplication>
#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include <QMap>

# ifdef linux
#   include <sys/vfs.h>
#   include <sys/statvfs.h>
#   include <sys/sysinfo.h>
# else
#   ifdef __FreeBSD__
#     include <sys/mount.h>
#   endif
#   if CONFIG_CYGWIN
#     include <sys/statfs.h>
#   endif
#   ifndef _WIN32
#     include <sys/sysctl.h>
#   endif
# endif

// libmythdb headers
#include "mythcorecontext.h"
#include "mythevent.h"
#include "mythverbose.h"
#include "exitcodes.h"

#ifdef USE_LIRC
#include "lircevent.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenuevent.h"
#endif

#ifndef USING_MINGW
typedef struct {
    QMutex  mutex;
    uint    result;
    time_t  timeout;
} PidData_t;

typedef QMap<pid_t, PidData_t *> PidMap_t;

class MythSystemReaper : public QThread
{
    public:
        void run(void);
        uint waitPid( pid_t pid, time_t timeout );
        uint abortPid( pid_t pid );
    private:
        PidMap_t    m_pidMap;
        QMutex      m_mapLock;
};

static class MythSystemReaper *reaper = NULL;

void MythSystemReaper::run(void)
{
    VERBOSE(VB_IMPORTANT, "Starting reaper thread");

    while( 1 ) {
        usleep(100000);

        time_t              now = time(NULL);
        PidMap_t::iterator  i, next;
        PidData_t          *pidData;
        pid_t               pid;
        int                 count;

        m_mapLock.lock();
        count = m_pidMap.size();
        if( !count )
        {
            m_mapLock.unlock();
            continue;
        }

        for( i = m_pidMap.begin(); i != m_pidMap.end(); i = next )
        {
            next    = i + 1;
            pidData = i.value();
            if( pidData->timeout == 0 || pidData->timeout > now )
                continue;

            // Timed out
            pid = i.key();

            next = m_pidMap.erase(i);
            pidData->result = GENERIC_EXIT_TIMEOUT;
            VERBOSE(VB_IMPORTANT, QString("Child PID %1 timed out, killing")
                .arg(pid));
            kill(pid, SIGTERM);
            usleep(500);
            kill(pid, SIGKILL);
            pidData->mutex.unlock();
        }
        count = m_pidMap.size();
        m_mapLock.unlock();

        if (!count)
            continue;

        /* There's at least one child to wait on */
        int         status;

        pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0)
        {
            if (pid < 0)
                VERBOSE(VB_IMPORTANT, QString("waitpid() failed because %1")
                    .arg(strerror(errno)));
            continue;
        }

        m_mapLock.lock();
        if (!m_pidMap.contains(pid))
        {
            VERBOSE(VB_IMPORTANT, QString("Child PID %1 not found in map!")
                .arg(pid));
            m_mapLock.unlock();
            continue;
        }

        pidData = m_pidMap.value(pid);
        m_pidMap.remove(pid);
        m_mapLock.unlock();

        if( WIFEXITED(status) )
        {
            pidData->result = WEXITSTATUS(status);
            VERBOSE(VB_IMPORTANT, QString("PID %1: exited: status=%2, result=%3")
                .arg(pid) .arg(status) .arg(pidData->result));
        }
        else if( WIFSIGNALED(status) )
        {
            pidData->result = GENERIC_EXIT_SIGNALLED;
            VERBOSE(VB_IMPORTANT, QString("PID %1: signal: status=%2, result=%3, signal=%4")
                .arg(pid) .arg(status) .arg(pidData->result) 
                .arg(WTERMSIG(status)));
        }
        else
        {
            pidData->result = GENERIC_EXIT_NOT_OK;
            VERBOSE(VB_IMPORTANT, QString("PID %1: other: status=%2, result=%3")
                .arg(pid) .arg(status) .arg(pidData->result));
        }

        pidData->mutex.unlock();
    }
}

uint MythSystemReaper::waitPid( pid_t pid, time_t timeout )
{
    PidData_t  *pidData = new PidData_t;
    uint        result;
    time_t      now;

    if( timeout > 0 )
    {
        now = time(NULL);
        pidData->timeout = now + timeout;
    }
    else
        pidData->timeout = 0;

    pidData->mutex.lock();
    m_mapLock.lock();
    m_pidMap.insert( pid, pidData );
    m_mapLock.unlock();

    /* Now we wait for the thread to see the SIGCHLD */
    pidData->mutex.lock();
    result = pidData->result;
    delete pidData;

    return( result );
}

uint MythSystemReaper::abortPid( pid_t pid )
{
    PidData_t  *pidData;
    uint        result;

    m_mapLock.lock();
    if (!m_pidMap.contains(pid))
    {
        VERBOSE(VB_IMPORTANT, QString("Child PID %1 not found in map!")
            .arg(pid));
        m_mapLock.unlock();
        return GENERIC_EXIT_NOT_OK;
    }

    pidData = m_pidMap.value(pid);
    m_pidMap.remove(pid);
    m_mapLock.unlock();

    delete pidData;

    VERBOSE(VB_IMPORTANT, QString("Child PID %1 aborted, killing") .arg(pid));
    kill(pid, SIGTERM);
    usleep(500);
    kill(pid, SIGKILL);
    result = GENERIC_EXIT_ABORTED;
    return( result );
}
#endif



/** \fn myth_system(const QString&, int, uint)
 *  \brief Runs a system command inside the /bin/sh shell.
 *
 *  Note: Returns GENERIC_EXIT_NOT_OK if it can not execute the command.
 *  \return Exit value from command as an unsigned int in range [0,255].
 */
uint myth_system(const QString &command, int flags, uint timeout)
{
    uint    result;
    bool    ready_to_lock;

    myth_system_pre_flags( flags, ready_to_lock );
#ifndef USING_MINGW
    pid_t   pid;

    pid    = myth_system_fork( command, result );

    if( result == GENERIC_EXIT_RUNNING )
        result = myth_system_wait( pid, timeout );
#else
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    QString cmd = QString("cmd.exe /c %1").arg(command);
    if (!::CreateProcessA(NULL, cmd.toUtf8().data(), NULL, NULL,
                          FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        VERBOSE(VB_IMPORTANT, (LOC_ERR + "CreateProcess() failed because %1")
                .arg(::GetLastError()));
        result = MYTHSYSTEM__EXIT__EXECL_ERROR;
    }
    else
    {
        if (::WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            VERBOSE(VB_IMPORTANT,
                    (LOC_ERR + "WaitForSingleObject() failed because %1")
                    .arg(::GetLastError()));
        DWORD exitcode = GENERIC_EXIT_OK;
        if (!GetExitCodeProcess(pi.hProcess, &exitcode))
            VERBOSE(VB_IMPORTANT, (LOC_ERR + 
                    "GetExitCodeProcess() failed because %1")
                    .arg(::GetLastError()));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        result = exitcode;
    }
#endif

    myth_system_post_flags( flags, ready_to_lock );

    return result;
}


#ifndef USING_MINGW
void myth_system_pre_flags(int &flags, bool &ready_to_lock)
{
    ready_to_lock = gCoreContext->HasGUI() && gCoreContext->IsUIThread();

#ifdef USE_LIRC
    bool lirc_lock_flag = !(flags & MYTH_SYSTEM_DONT_BLOCK_LIRC);
    LircEventLock lirc_lock(lirc_lock_flag && ready_to_lock);
#endif

#ifdef USE_JOYSTICK_MENU
    bool joy_lock_flag = !(flags & MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU);
    JoystickMenuEventLock joystick_lock(joy_lock_flag && ready_to_lock);
#endif

#ifdef BSD
    // Darwin waitpid() frequently fails EINTR (interrupted system call) -
    // I think because the parent is being toggled between kernel sleep/wake.
    // This seems to work around whatever is causing this 
    flags |= MYTH_SYSTEM_DONT_BLOCK_PARENT;
#endif

    // This needs to be a send event so that the MythUI m_drawState change is
    // flagged immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if (ready_to_lock && !(flags & MYTH_SYSTEM_DONT_BLOCK_PARENT))
    {
        QEvent event(MythEvent::kPushDisableDrawingEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }
}


void myth_system_post_flags(int &flags, bool &ready_to_lock)
{
    // This needs to be a send event so that the MythUI m_drawState change is
    // flagged immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if (ready_to_lock && !(flags & MYTH_SYSTEM_DONT_BLOCK_PARENT))
    {
        QEvent event(MythEvent::kPopDisableDrawingEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }
}


pid_t myth_system_fork(const QString &command, uint &result)
{
    QString LOC_ERR = QString("myth_system('%1'): Error: ").arg(command);
    VERBOSE(VB_IMPORTANT, QString("Launching: %1") .arg(command));
    pid_t child = fork();

    if (child < 0)
    {
        /* Fork failed */
        VERBOSE(VB_IMPORTANT, (LOC_ERR + "fork() failed because %1")
                .arg(strerror(errno)));
        result = GENERIC_EXIT_NOT_OK;
        return -1;
    }
    else if (child == 0)
    {
        /* Child */
        /* In case we forked WHILE it was locked */
        bool unlocked = verbose_mutex.tryLock();
        verbose_mutex.unlock();
        if( !unlocked )
            VERBOSE(VB_IMPORTANT, "Cleared parent's verbose lock");

        /* Close all open file descriptors except stdout/stderr */
        for (int i = sysconf(_SC_OPEN_MAX) - 1; i > 2; i--)
            close(i);

        /* Try to attach stdin to /dev/null */
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0)
        {
            // Note: dup2() will close old stdin descriptor.
            if (dup2(fd, 0) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Can not redirect /dev/null to standard input,"
                        "\n\t\t\tfailed to duplicate file descriptor." + ENO);
            }
            close(fd);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Can not redirect /dev/null "
                    "to standard input, failed to open." + ENO);
        }

        /* Run command */
        execl("/bin/sh", "sh", "-c", command.toUtf8().constData(), (char *)0);
        if (errno)
        {
            VERBOSE(VB_IMPORTANT, (LOC_ERR + "execl() failed because %1")
                    .arg(strerror(errno)));
        }

        /* Failed to exec */
        _exit(MYTHSYSTEM__EXIT__EXECL_ERROR); // this exit is ok
    }

    /* Parent */
    result = GENERIC_EXIT_RUNNING;
    return child;
}

uint myth_system_wait(pid_t pid, uint timeout)
{
    if( reaper == NULL )
    {
        reaper = new MythSystemReaper;
        reaper->start();
    }
    VERBOSE(VB_IMPORTANT, QString("PID %1: launched") .arg(pid));
    return reaper->waitPid(pid, timeout);
}

uint myth_system_abort(pid_t pid)
{
    if( reaper == NULL )
    {
        reaper = new MythSystemReaper;
        reaper->start();
    }
    VERBOSE(VB_IMPORTANT, QString("PID %1: aborted") .arg(pid));
    return reaper->abortPid(pid);
}
#endif


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
