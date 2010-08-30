
// Own header
#include "mythsystem.h"

// compat header
#include "compat.h"

// C++/C headers
#include <cerrno>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

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
} PidData_t;

typedef QMap<pid_t, PidData_t *> PidMap_t;

class MythSystemReaper : public QThread
{
    public:
        void run(void);
        uint waitPid( pid_t pid );
    private:
        PidMap_t    m_pidMap;
        QSemaphore  m_sem;
};

static class MythSystemReaper *reaper = NULL;

void MythSystemReaper::run(void)
{
    VERBOSE(VB_IMPORTANT, "Starting reaper thread");

    while( 1 ) {
        usleep(100000);

        m_sem.acquire(1);

        /* There's at least one child to wait on */
        pid_t       res;
        int         status;

        res = waitpid(-1, &status, WNOHANG);
        if (res <= 0)
        {
            if( res < 0 )
                VERBOSE(VB_IMPORTANT, QString("waitpid() failed because %1")
                    .arg(strerror(errno)));
            m_sem.release(1);
            continue;
        }

        if (!m_pidMap.contains(res))
        {
            VERBOSE(VB_IMPORTANT, QString("Child PID %1 not found in map!")
                .arg(res));
            continue;
        }

        PidData_t  *pidData = m_pidMap.value(res);
        m_pidMap.remove(res);

        if( WIFEXITED(status) )
        {
            pidData->result = WEXITSTATUS(status);
            VERBOSE(VB_IMPORTANT, QString("PID %1: exited: status=%2, result=%3")
                .arg(res) .arg(status) .arg(pidData->result));
        }
        else if( WIFSIGNALED(status) )
        {
            pidData->result = GENERIC_EXIT_SIGNALLED;
            VERBOSE(VB_IMPORTANT, QString("PID %1: signal: status=%2, result=%3, signal=%4")
                .arg(res) .arg(status) .arg(pidData->result) 
                .arg(WTERMSIG(status)));
        }
        else
        {
            pidData->result = GENERIC_EXIT_NOT_OK;
            VERBOSE(VB_IMPORTANT, QString("PID %1: other: status=%2, result=%3")
                .arg(res) .arg(status) .arg(pidData->result));
        }

        pidData->mutex.unlock();
    }
}

uint MythSystemReaper::waitPid( pid_t pid )
{
    PidData_t  *pidData = new PidData_t;
    uint        result;

    pidData->mutex.lock();
    m_pidMap.insert( pid, pidData );
    m_sem.release(1);

    /* Now we wait for the thread to see the SIGCHLD */
    pidData->mutex.lock();
    result = pidData->result;
    delete pidData;

    return( result );
}
#endif

/** \fn myth_system(const QString&, int)
 *  \brief Runs a system command inside the /bin/sh shell.
 *
 *  Note: Returns GENERIC_EXIT_NOT_OK if it can not execute the command.
 *  \return Exit value from command as an unsigned int in range [0,255].
 */
uint myth_system(const QString &command, int flags)
{
    (void)flags; /* Kill warning */

    bool ready_to_lock = gCoreContext->HasGUI() && gCoreContext->IsUIThread();

    uint result = GENERIC_EXIT_NOT_OK;

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

    QString LOC_ERR = QString("myth_system('%1'): Error: ").arg(command);

#ifndef USING_MINGW
    VERBOSE(VB_IMPORTANT, QString("Launching: %1") .arg(command));
    pid_t child = fork();

    if (child < 0)
    {
        /* Fork failed */
        VERBOSE(VB_IMPORTANT, (LOC_ERR + "fork() failed because %1")
                .arg(strerror(errno)));
        result = GENERIC_EXIT_NOT_OK;
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
    else
    {
        /* Parent */
        if( reaper == NULL )
        {
            reaper = new MythSystemReaper;
            reaper->start();
        }
        VERBOSE(VB_IMPORTANT, QString("PID %1: launched")
            .arg(child));
        result = reaper->waitPid(child);
    }

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

    // This needs to be a send event so that the MythUI m_drawState change is
    // flagged immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if (ready_to_lock && !(flags & MYTH_SYSTEM_DONT_BLOCK_PARENT))
    {
        QEvent event(MythEvent::kPopDisableDrawingEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    return result;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
