
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
#include <signal.h>  // for kill()

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

#ifndef USING_MINGW
typedef struct {
    QMutex  mutex;
    uint    result;
    time_t  timeout;
    bool    background;
} PidData_t;

typedef QMap<pid_t, PidData_t *> PidMap_t;

class MythSystemReaper : public QThread
{
    public:
        void run(void);
        uint waitPid( pid_t pid, time_t timeout, bool background = false,
                      bool processEvents = true );
        uint abortPid( pid_t pid );
    private:
        PidMap_t    m_pidMap;
        QMutex      m_mapLock;
};

static class MythSystemReaper *reaper = NULL;

void MythSystemReaper::run(void)
{
    VERBOSE(VB_GENERAL, "Starting reaper thread");

    // gCoreContext is set to NULL during shutdown, and we need this thread to
    // exit during shutdown.
    while( gCoreContext ) {
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
            VERBOSE(VB_GENERAL, QString("Child PID %1 timed out, killing")
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
                VERBOSE(VB_GENERAL, QString("waitpid() failed because %1")
                    .arg(strerror(errno)));
            continue;
        }

        m_mapLock.lock();
        if (!m_pidMap.contains(pid))
        {
            VERBOSE(VB_GENERAL, QString("Child PID %1 not found in map!")
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
            VERBOSE(VB_GENERAL, QString("PID %1: exited: status=%2, result=%3")
                .arg(pid) .arg(status) .arg(pidData->result));
        }
        else if( WIFSIGNALED(status) )
        {
            pidData->result = GENERIC_EXIT_SIGNALLED;
            VERBOSE(VB_GENERAL, QString("PID %1: signal: status=%2, result=%3, signal=%4")
                .arg(pid) .arg(status) .arg(pidData->result) 
                .arg(WTERMSIG(status)));
        }
        else
        {
            pidData->result = GENERIC_EXIT_NOT_OK;
            VERBOSE(VB_GENERAL, QString("PID %1: other: status=%2, result=%3")
                .arg(pid) .arg(status) .arg(pidData->result));
        }

        if( pidData->background )
            delete pidData;
        else
            pidData->mutex.unlock();
    }
}

uint MythSystemReaper::waitPid( pid_t pid, time_t timeout, bool background,
                                bool processEvents )
{
    PidData_t  *pidData = new PidData_t;
    uint        result;

    if( timeout > 0 )
        pidData->timeout = time(NULL) + timeout;
    else
        pidData->timeout = 0;

    pidData->background = background;

    pidData->mutex.lock();
    m_mapLock.lock();
    m_pidMap.insert( pid, pidData );
    m_mapLock.unlock();

    if( !background ) {
        /* Now we wait for the thread to see the SIGCHLD */
        while( !pidData->mutex.tryLock(100) )
            if (processEvents)
                qApp->processEvents();

        result = pidData->result;
        delete pidData;

        return( result );
    }

    /* We are running in the background, the reaper will still catch the 
       child */
    return( GENERIC_EXIT_OK );
}

uint MythSystemReaper::abortPid( pid_t pid )
{
    PidData_t  *pidData;
    uint        result;

    m_mapLock.lock();
    if (!m_pidMap.contains(pid))
    {
        VERBOSE(VB_GENERAL, QString("Child PID %1 not found in map!")
            .arg(pid));
        m_mapLock.unlock();
        return GENERIC_EXIT_NOT_OK;
    }

    pidData = m_pidMap.value(pid);
    m_pidMap.remove(pid);
    m_mapLock.unlock();

    delete pidData;

    VERBOSE(VB_GENERAL, QString("Child PID %1 aborted, killing") .arg(pid));
    kill(pid, SIGTERM);
    usleep(500);
    kill(pid, SIGKILL);
    result = GENERIC_EXIT_ABORTED;
    return( result );
}
#endif



/** \fn myth_system(const QString&, uint, uint)
 *  \brief Runs a system command inside the /bin/sh shell.
 *
 *  Note: Returns GENERIC_EXIT_NOT_OK if it cannot execute the command.
 *  \return Exit value from command as an unsigned int in range [0,255].
 */
uint myth_system(const QString &command, uint flags, uint timeout)
{
    uint            result;

    if( !(flags & kMSRunBackground) && command.endsWith("&") )
    {
        VERBOSE(VB_GENERAL, "Adding background flag");
        flags |= kMSRunBackground;
    }

    myth_system_pre_flags( flags );

#ifndef USING_MINGW
    pid_t   pid;

    pid    = myth_system_fork( command, result );

    if( result == GENERIC_EXIT_RUNNING )
        result = myth_system_wait( pid, timeout, (flags & kMSRunBackground),
                                   (flags & kMSProcessEvents) );
#else
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    QString LOC_ERR = QString("myth_system('%1'): Error: ").arg(command);

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    QString cmd = QString("cmd.exe /c %1").arg(command);
    if (!::CreateProcessA(NULL, cmd.toUtf8().data(), NULL, NULL,
                          FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        VERBOSE(VB_GENERAL, (LOC_ERR + "CreateProcess() failed because %1")
                .arg(::GetLastError()));
        result = MYTHSYSTEM__EXIT__EXECL_ERROR;
    }
    else
    {
        if (::WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            VERBOSE(VB_GENERAL,
                    (LOC_ERR + "WaitForSingleObject() failed because %1")
                    .arg(::GetLastError()));
        DWORD exitcode = GENERIC_EXIT_OK;
        if (!GetExitCodeProcess(pi.hProcess, &exitcode))
            VERBOSE(VB_GENERAL, (LOC_ERR + 
                    "GetExitCodeProcess() failed because %1")
                    .arg(::GetLastError()));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        result = exitcode;
    }
#endif

    myth_system_post_flags( flags );

    return result;
}


void myth_system_pre_flags(uint &flags)
{
    bool isInUi = gCoreContext->HasGUI() && gCoreContext->IsUIThread();
    if( isInUi )
    {
        flags |= kMSInUi;
    }
    else
    {
        // These locks only happen in the UI, and only if the flags are cleared
        // so as we are not in the UI, set the flags to simplify logic further
        // down
        flags &= ~kMSInUi;
        flags |= kMSDontBlockInputDevs;
        flags |= kMSDontDisableDrawing;
        flags &= ~kMSProcessEvents;
    }

    if (flags & kMSRunBackground )
        flags &= ~kMSProcessEvents;

    // This needs to be a send event so that the MythUI locks the input devices
    // immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if( !(flags & kMSDontBlockInputDevs) )
    {
        QEvent event(MythEvent::kLockInputDevicesEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    // This needs to be a send event so that the MythUI m_drawState change is
    // flagged immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if( !(flags & kMSDontDisableDrawing) )
    {
        QEvent event(MythEvent::kPushDisableDrawingEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }
}

void myth_system_post_flags(uint &flags)
{
    // This needs to be a send event so that the MythUI m_drawState change is
    // flagged immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if( !(flags & kMSDontDisableDrawing) )
    {
        QEvent event(MythEvent::kPopDisableDrawingEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    // This needs to be a post event so that the MythUI unlocks input devices
    // after all existing (blocked) events are processed and ignored.
    if( !(flags & kMSDontBlockInputDevs) )
    {
        QEvent *event = new QEvent(MythEvent::kUnlockInputDevicesEventType);
        QCoreApplication::postEvent(gCoreContext->GetGUIObject(), event);
    }
}


#ifndef USING_MINGW
pid_t myth_system_fork(const QString &command, uint &result)
{
    QString LOC_ERR = QString("myth_system('%1'): Error: ").arg(command);
    VERBOSE(VB_GENERAL, QString("Launching: %1") .arg(command));
    pid_t child = fork();

    if (child < 0)
    {
        /* Fork failed */
        VERBOSE(VB_GENERAL, (LOC_ERR + "fork() failed because %1")
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
            VERBOSE(VB_GENERAL, "Cleared parent's verbose lock");

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
                VERBOSE(VB_GENERAL, LOC_ERR +
                        "Cannot redirect /dev/null to standard input,"
                        "\n\t\t\tfailed to duplicate file descriptor." + ENO);
            }
            close(fd);
        }
        else
        {
            VERBOSE(VB_GENERAL, LOC_ERR + "Cannot redirect /dev/null "
                    "to standard input, failed to open." + ENO);
        }

        /* Run command */
        execl("/bin/sh", "sh", "-c", command.toUtf8().constData(), (char *)0);
        if (errno)
        {
            VERBOSE(VB_GENERAL, (LOC_ERR + "execl() failed because %1")
                    .arg(strerror(errno)));
        }

        /* Failed to exec */
        _exit(MYTHSYSTEM__EXIT__EXECL_ERROR); // this exit is ok
    }

    /* Parent */
    result = GENERIC_EXIT_RUNNING;
    return child;
}

uint myth_system_wait(pid_t pid, uint timeout, bool background, 
                      bool processEvents)
{
    if( reaper == NULL )
    {
        reaper = new MythSystemReaper;
        reaper->start();
    }
    VERBOSE(VB_GENERAL, QString("PID %1: launched%2") .arg(pid)
        .arg(background ? " in the background, not waiting" : ""));
    return reaper->waitPid(pid, timeout, background, processEvents);
}

uint myth_system_abort(pid_t pid)
{
    if( reaper == NULL )
    {
        reaper = new MythSystemReaper;
        reaper->start();
    }
    VERBOSE(VB_GENERAL, QString("PID %1: aborted") .arg(pid));
    return reaper->abortPid(pid);
}
#endif


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
