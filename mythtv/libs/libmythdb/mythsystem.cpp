
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
#include <pthread.h>
#include <sys/select.h>
#include <sys/wait.h>

// QT headers
#include <QCoreApplication>
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

// libmythdb headers
#include "mythcorecontext.h"
#include "mythevent.h"
#include "mythverbose.h"
#include "exitcodes.h"

#define CLOSE(x) if( x >= 0 ) {close(x); x = -1;}

typedef QList<MythSystem *> MSList;

/**********************************
 * MythSystemManager method defines
 *********************************/
static class MythSystemManager *manager = NULL;

MythSystemIOHandler::MythSystemIOHandler(bool read) :
    QThread(), m_read(read)
{
}

void MythSystemIOHandler::run(void)
{
    m_pLock.lock();
    BuildFDs();
    m_pLock.unlock();

    QMutex mutex;

    while( gCoreContext )
    {
        mutex.lock();
        m_pWait.wait(&mutex);
        mutex.unlock();

        while( gCoreContext )
        {
            usleep(10000); // ~100x per second, for ~3MBps throughput
            m_pLock.lock();
            if( m_pMap.isEmpty() )
            {
                m_pLock.unlock();
                break;
            }
            
            timeval tv;
            tv.tv_sec = 0; tv.tv_usec = 0;
          
            int retval;
            fd_set fds = m_fds;

            if( m_read )
                retval = select(m_maxfd+1, &fds, NULL, NULL, &tv);
            else
                retval = select(m_maxfd+1, NULL, &fds, NULL, &tv);

            if( retval == -1 )
                VERBOSE(VB_GENERAL, QString("select() failed because of %1")
                            .arg(strerror(errno)));

            else if( retval > 0 )
            {
                PMap_t::iterator i, next;
                for( i = m_pMap.begin(); i != m_pMap.end(); i = next )
                {
                    next = i+1;
                    int fd = i.key();
                    if( FD_ISSET(fd, &fds) )
                    {
                        if( m_read )
                            HandleRead(i.key(), i.value());
                        else
                            HandleWrite(i.key(), i.value());
                    }
                }
            }
            m_pLock.unlock();
        }
    }
}

void MythSystemIOHandler::HandleRead(int fd, QBuffer *buff)
{
    int len;
    errno = 0;
    if( (len = read(fd, &m_readbuf, 65536)) <= 0 )
    {
        if( errno != EAGAIN )
        {
            m_pMap.remove(fd);
            BuildFDs();
        }
    }
    else
        buff->buffer().append(m_readbuf, len);
}

void MythSystemIOHandler::HandleWrite(int fd, QBuffer *buff)
{
    if( buff->atEnd() )
    {
        m_pMap.remove(fd);
        BuildFDs();
        return;
    }

    int pos = buff->pos();
    int len = buff->size() - pos;
    len = (len > 32768 ? 32768 : len);

    int rlen = write(fd, buff->read(len).constData(), len);
    if( rlen < 0 )
    {
        if( errno != EAGAIN )
        {
            m_pMap.remove(fd);
            BuildFDs();
        }
        else
            buff->seek(pos);
    }
    else if( rlen != len )
        buff->seek(pos+rlen);
}

void MythSystemIOHandler::insert(int fd, QBuffer *buff)
{
    m_pLock.lock();
    m_pMap.insert(fd, buff);
    BuildFDs();
    m_pLock.unlock();
    wake();
}

void MythSystemIOHandler::remove(int fd)
{
    m_pLock.lock();
    m_pMap.remove(fd);
    BuildFDs();
    m_pLock.unlock();
}

void MythSystemIOHandler::wake()
{
    m_pWait.wakeAll();
}

void MythSystemIOHandler::BuildFDs()
{
    // build descriptor list
    FD_ZERO(&m_fds);
    m_maxfd = -1;

    PMap_t::iterator i;
    for( i = m_pMap.begin(); i != m_pMap.end(); ++i )
    {
        FD_SET(i.key(), &m_fds);
        m_maxfd = (i.key() > m_maxfd ? i.key() : m_maxfd);
    }
}

MythSystemManager::MythSystemManager() :
        QThread(), m_primary(true)
{
}

MythSystemManager::MythSystemManager(MythSystemManager *other) :
        QThread()
{
    m_msList =   other->m_msList;
    m_listLock = other->m_listLock;
    m_primary =  false;
}

void MythSystemManager::run(void)
{
    if( m_primary )
        RunManagerThread();
    else
        RunSignalThread();
}

void MythSystemManager::RunManagerThread()
{
    VERBOSE(VB_GENERAL, "Starting process manager");

    m_msList = new MSList();
    m_listLock = new QMutex();

    MythSystemManager *sthread = new MythSystemManager(this);
    sthread->start();

    m_readThread =  new MythSystemIOHandler(true);
    m_writeThread = new MythSystemIOHandler(false);

    // gCoreContext is set to NULL during shutdown, and we need this thread to
    // exit during shutdown.
    while( gCoreContext )
    {
        usleep(100000); // sleep 100ms

        // check for any running processes
        m_mapLock.lock();

        if( m_pMap.isEmpty() )
        {
            m_mapLock.unlock();
            continue;
        }
        m_mapLock.unlock();

        m_listLock->lock();
        MythSystem         *ms;
        pid_t               pid;

        // check for any newly exited processes
        int status;
        while( (pid = waitpid(-1, &status, WNOHANG)) > 0 )
        {
            m_mapLock.lock();
            // unmanaged process has exited
            if( !m_pMap.contains(pid) )
            {
                VERBOSE(VB_GENERAL, QString("Unmanaged child (PID: %1) has exited!")
                    .arg(pid));
                m_mapLock.unlock();
                continue;
            }

            // pop exited process off managed list, add to cleanup list
            ms = m_pMap.take(pid);
            m_mapLock.unlock();
            m_msList->append(ms);

            // handle normal exit
            if( WIFEXITED(status) )
            {
                ms->m_status = WEXITSTATUS(status);
                VERBOSE(VB_GENERAL, QString("Managed child (PID: %1) has exited! "
                                            "command=%2, status=%3, result=%4")
                    .arg(pid) .arg(ms->m_logcmd) .arg(status) .arg(ms->m_status));
            }

            // handle forced exit
            else if( WIFSIGNALED(status) && 
                     ms->m_status != GENERIC_EXIT_TIMEOUT )
            {
                // Don't override a timed out process which gets killed, but
                // otherwise set the return status appropriately
                int sig = WTERMSIG(status);
                if( sig == 9 )
                    ms->m_status = GENERIC_EXIT_ABORTED;
                else if( sig == 11 )
                    ms->m_status = GENERIC_EXIT_TERMINATED;
                else
                    ms->m_status = GENERIC_EXIT_SIGNALLED;

                VERBOSE(VB_GENERAL, QString("Managed child (PID: %1) has signalled! "
                                            "command=%2, status=%3, result=%4, signal=%5")
                    .arg(pid) .arg(ms->m_logcmd) .arg(status) .arg(ms->m_status)
                    .arg(sig));
            }

            // handle abnormal exit (crash)
            else
            {
                ms->m_status = GENERIC_EXIT_NOT_OK;
                VERBOSE(VB_GENERAL, QString("Managed child (PID: %1) has terminated! "
                                            "command=%2, status=%3, result=%4")
                    .arg(pid) .arg(ms->m_logcmd) .arg(status) .arg(ms->m_status));
            }

        }


        // loop through running processes for any that require action
        MSMap_t::iterator   i, next;
        time_t              now = time(NULL);
        m_mapLock.lock();
        for( i = m_pMap.begin(); i != m_pMap.end(); i = next )
        {
            next = i + 1;
            pid  = i.key();
            ms   = i.value();

            // handle processes beyond marked timeout
            if( ms->m_timeout > 0 && ms->m_timeout < now )
            {
                // issuing KILL signal after TERM failed in a timely manner
                if( ms->m_status == GENERIC_EXIT_TIMEOUT )
                {
                    VERBOSE(VB_GENERAL, QString("Managed child (PID: %1) timed out"
                                                ", issuing KILL signal").arg(pid));
                    // Prevent constant attempts to kill an obstinate child
                    ms->m_timeout = 0;
                    ms->Kill();
                }

                // issuing TERM signal
                else
                {
                    VERBOSE(VB_GENERAL, QString("Managed child (PID: %1) timed out"
                                                ", issuing TERM signal").arg(pid));
                    ms->m_status = GENERIC_EXIT_TIMEOUT;
                    ms->m_timeout = now + 1;
                    ms->Term();
                }
            }
        }

        m_mapLock.unlock();

        // hold off unlocking until all the way down here to 
        // give the buffer handling a chance to run before
        // being closed down by signal thread
        m_listLock->unlock();
    }

    // kick to allow them to close themselves cleanly
    m_readThread->wake();
    m_writeThread->wake();
}

// spawn separate thread for signals to prevent manager
// thread from blocking in some slot
void MythSystemManager::RunSignalThread(void)
{
    VERBOSE(VB_GENERAL, "Starting process signal handler");
    while( gCoreContext )
    {
        usleep(50000); // sleep 50ms
        while( gCoreContext )
        {
            // handle cleanup and signalling for closed processes
            m_listLock->lock();
            if( m_msList->isEmpty() )
            {
                m_listLock->unlock();
                break;
            }
            MythSystem *ms = m_msList->takeFirst();
            m_listLock->unlock();

            ms->HandlePostRun();
            CLOSE(ms->m_stdpipe[0]);
            CLOSE(ms->m_stdpipe[1]);
            CLOSE(ms->m_stdpipe[2]);
            ms->m_pmutex.unlock();

            if( ms->m_status == GENERIC_EXIT_OK )
                emit ms->finished();
            else
                emit ms->error(ms->m_status);
        }
    }
}

void MythSystemManager::append(MythSystem *ms)
{
    m_mapLock.lock();
    m_pMap.insert(ms->m_pid, ms);
    m_mapLock.unlock();

    if( ms->m_usestdin )
        m_writeThread->insert(ms->m_stdpipe[0], &(ms->m_stdbuff[0]));
    if( ms->m_usestdout )
        m_readThread->insert(ms->m_stdpipe[1], &(ms->m_stdbuff[1]));
    if( ms->m_usestderr )
        m_readThread->insert(ms->m_stdpipe[2], &(ms->m_stdbuff[2]));
}

/*******************************
 * MythSystem method defines
 ******************************/

MythSystem::MythSystem(const QString &command, uint flags)
{
    SetCommand(command, flags);
}

/** \fn MythSystem::setCommand(const QString &command) 
 *  \brief Resets an existing MythSystem object to a new command
 */
void MythSystem::SetCommand(const QString &command, uint flags)
{
    m_status = GENERIC_EXIT_START;
    // force shell operation
    flags |= kMSRunShell;
    ProcessFlags(flags);
    m_command = QString(command);
    m_logcmd = m_command;
    if( flags & kMSAnonLog ) 
    {
        m_logcmd.truncate(m_logcmd.indexOf(" "));
        m_logcmd.append(" (anonymized)");
    }
}


MythSystem::MythSystem(const QString &command, 
                       const QStringList &args, uint flags)
{
    SetCommand(command, args, flags);
}

/** \fn MythSystem::setCommand(const QString &command, 
                               const QStringList &args)
 *  \brief Resets an existing MythSystem object to a new command
 */
void MythSystem::SetCommand(const QString &command, 
                            const QStringList &args, uint flags)
{
    m_status = GENERIC_EXIT_START;
    m_command = QString(command);

    ProcessFlags(flags);

    if( m_useshell )
        m_command.append(args.join(" "));

    else
    {
        // check for execute rights
        if( !access(command.toUtf8().constData(), X_OK) )
            m_status = GENERIC_EXIT_CMD_NOT_FOUND;
        else
            m_args = QStringList(args);
    }

    m_logcmd = m_command;
    if( flags & kMSAnonLog )
    {
        m_logcmd.truncate(m_logcmd.indexOf(" "));
        m_logcmd.append(" (anonymized)");
    }
}


MythSystem::MythSystem(const MythSystem &other) :
    m_status(other.m_status),
    m_pid(other.m_pid),
    m_timeout(other.m_timeout),

    m_command(other.m_command),
    m_logcmd(other.m_logcmd),
    m_args(other.m_args),

    m_runinbackground(other.m_runinbackground),
    m_isinui(other.m_isinui),
    m_blockinputdevs(other.m_blockinputdevs),
    m_disabledrawing(other.m_disabledrawing),
    m_processevents(other.m_processevents),
    m_usestdin(other.m_usestdin),
    m_usestdout(other.m_usestdout),
    m_usestderr(other.m_usestderr),
    m_useshell(other.m_useshell)
{
}

// QBuffers may also need freeing
MythSystem::~MythSystem(void)
{
}


void MythSystem::SetDirectory(const QString &directory)
{
    m_setdirectory = true;
    m_directory = QString(directory);
}

/** \fn MythSystem::Run()
 *  \brief Runs a command inside the /bin/sh shell. Returns immediately
 */
void MythSystem::Run(time_t timeout)
{
    // runs pre_flags
    // forks child process
    // spawns manager and hand off self
    if( m_status != GENERIC_EXIT_START )
    {
        emit error(m_status);
        return;
    }

    HandlePreRun();

    m_timeout = timeout;
    Fork();

    if( manager == NULL )
    {
        manager = new MythSystemManager;
        manager->start();
    }

    if( timeout )
        m_timeout += time(NULL);

    m_pmutex.lock();
    emit started();
    manager->append(this);
}

// should there be a separate 'getstatus' call? or is using
// Wait() for that purpose sufficient?
uint MythSystem::Wait(time_t timeout)
{
    if( (m_status != GENERIC_EXIT_RUNNING) || m_runinbackground )
        return m_status;

    if( m_processevents )
    {
        if( timeout > 0 )
            timeout += time(NULL);

        while( !timeout || time(NULL) < timeout )
        {
            // loop until timeout hits or process ends
            if( m_pmutex.tryLock(100) )
            {
                m_pmutex.unlock();
                break;
            }

            qApp->processEvents();
        }
    }
    else
    {
        if( timeout > 0 )
        {
            if( m_pmutex.tryLock(timeout*1000) )
                m_pmutex.unlock();
        }
        else
        {
            m_pmutex.lock();
            m_pmutex.unlock();
        }
    }
    return m_status;
}

void MythSystem::Term(bool force)
{
    if( (m_status != GENERIC_EXIT_RUNNING) || (m_pid <= 0) )
        return;
    VERBOSE(VB_GENERAL, QString("Child PID %1 aborted, terminating")
                    .arg(m_pid));
    // send TERM signal to process
    kill(m_pid, SIGTERM);
    if( force )
    {
        // send KILL if it does not exit within one second
        if( Wait(1) == GENERIC_EXIT_RUNNING )
            Kill();
    }
}

void MythSystem::Kill() const
{
    if( (m_status != GENERIC_EXIT_RUNNING) || (m_pid <= 0) )
        return;
    VERBOSE(VB_GENERAL, QString("Child PID %1 aborted, killing")
                    .arg(m_pid));
    kill(m_pid, SIGKILL);
}

void MythSystem::Stop() const
{
    if( (m_status != GENERIC_EXIT_RUNNING) || (m_pid <= 0) )
        return;
    VERBOSE(VB_GENERAL, QString("Child PID %1 suspended")
                    .arg(m_pid));
    kill(m_pid, SIGSTOP);
}

void MythSystem::Cont() const
{
    if( (m_status != GENERIC_EXIT_RUNNING) || (m_pid <= 0) )
        return;
    VERBOSE(VB_GENERAL, QString("Child PID %1 resumed")
                    .arg(m_pid));
    kill(m_pid, SIGCONT);
}

void MythSystem::HangUp() const
{
    if( (m_status != GENERIC_EXIT_RUNNING) || (m_pid <= 0) )
        return;
    VERBOSE(VB_GENERAL, QString("Child PID %1 hung-up")
                    .arg(m_pid));
    kill(m_pid, SIGHUP);
}

void MythSystem::USR1() const
{
    if( (m_status != GENERIC_EXIT_RUNNING) || (m_pid <= 0) )
        return;
    VERBOSE(VB_GENERAL, QString("Child PID %1 USR1")
                    .arg(m_pid));
    kill(m_pid, SIGUSR1);
}

void MythSystem::USR2() const
{
    if( (m_status != GENERIC_EXIT_RUNNING) || (m_pid <= 0) )
        return;
    VERBOSE(VB_GENERAL, QString("Child PID %1 USR2")
                    .arg(m_pid));
    kill(m_pid, SIGUSR2);
}

bool MythSystem::isBackground() const
{
    return m_runinbackground;
}

void MythSystem::ProcessFlags(uint flags)
{
    m_runinbackground = false;
    m_isinui          = false;
    m_blockinputdevs  = false;
    m_disabledrawing  = false;
    m_processevents   = false;
    m_usestdin        = false;
    m_usestdout       = false;
    m_usestderr       = false;
    m_useshell        = false;
    m_setdirectory    = false;

    if( m_status != GENERIC_EXIT_START )
        return;

    if( flags & kMSRunBackground )
        m_runinbackground = true;
    else if( m_command.endsWith("&") )
    {
        VERBOSE(VB_GENERAL, "Adding background flag");
        m_runinbackground = true;
        m_useshell = true;
    }

    m_isinui = gCoreContext->HasGUI() && gCoreContext->IsUIThread();
    if( m_isinui )
    {
        // Check for UI-only locks
        m_blockinputdevs = !(flags & kMSDontBlockInputDevs);
        m_disabledrawing = !(flags & kMSDontDisableDrawing);
        m_processevents  = flags & kMSProcessEvents;
    }

    if( flags & kMSStdIn )
        m_usestdin = true;
    if( flags & kMSStdOut )
        m_usestdout = true;
    if( flags & kMSStdErr )
        m_usestderr = true;
    if( flags & kMSRunShell )
        m_useshell = true;
    if( flags & kMSNoRunShell ) // override for use with myth_system
        m_useshell = false;
}

QByteArray MythSystem::Read(int size)    { return m_stdbuff[1].read(size); }
QByteArray MythSystem::ReadErr(int size) { return m_stdbuff[2].read(size); }
QByteArray& MythSystem::ReadAll()        { return m_stdbuff[1].buffer(); }
QByteArray& MythSystem::ReadAllErr()     { return m_stdbuff[2].buffer(); }

int MythSystem::Write(const QByteArray &ba)
{
    if (!m_usestdin)
        return 0;

    m_stdbuff[0].buffer().append(ba.constData());
    return ba.size();
}


void MythSystem::HandlePreRun()
{
    // This needs to be a send event so that the MythUI locks the input devices
    // immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if( m_blockinputdevs )
    {
        QEvent event(MythEvent::kLockInputDevicesEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    // This needs to be a send event so that the MythUI m_drawState change is
    // flagged immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if( m_disabledrawing )
    {
        QEvent event(MythEvent::kPushDisableDrawingEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }
}

void MythSystem::HandlePostRun()
{
    // This needs to be a send event so that the MythUI m_drawState change is
    // flagged immediately instead of after existing events are processed
    // since this function could be called inside one of those events.
    if( m_disabledrawing )
    {
        QEvent event(MythEvent::kPopDisableDrawingEventType);
        QCoreApplication::sendEvent(gCoreContext->GetGUIObject(), &event);
    }

    // This needs to be a post event so that the MythUI unlocks input devices
    // after all existing (blocked) events are processed and ignored.
    if( m_blockinputdevs )
    {
        QEvent *event = new QEvent(MythEvent::kUnlockInputDevicesEventType);
        QCoreApplication::postEvent(gCoreContext->GetGUIObject(), event);
    }
}

#define MAX_BUFLEN 1024
void MythSystem::Fork()
{
    QString LOC_ERR = QString("myth_system('%1'): Error: ").arg(m_logcmd);

    // For use in the child
    char locerr[MAX_BUFLEN];
    strncpy(locerr, (const char *)LOC_ERR.constData(), MAX_BUFLEN);
    locerr[MAX_BUFLEN-1] = '\0';

    VERBOSE(VB_GENERAL, QString("Launching: %1").arg(m_logcmd));

    m_stdbuff[0].setBuffer(0);
    m_stdbuff[1].setBuffer(0);
    m_stdbuff[2].setBuffer(0);

    int p_stdin[]  = {-1,-1};
    int p_stdout[] = {-1,-1};
    int p_stderr[] = {-1,-1};

    /* set up pipes */
    if( m_usestdin )
    {
        if( pipe(p_stdin) == -1 )
        {
            VERBOSE(VB_GENERAL, (LOC_ERR + "stdin pipe() failed"));
            m_status = GENERIC_EXIT_NOT_OK;
        }
        else
            fcntl(p_stdin[1], F_SETFL, O_NONBLOCK);
    }
    if( m_usestdout )
    {
        if( pipe(p_stdout) == -1 )
        {
            VERBOSE(VB_GENERAL, (LOC_ERR + "stdout pipe() failed"));
            m_status = GENERIC_EXIT_NOT_OK;
        }
        else
            fcntl(p_stdout[0], F_SETFL, O_NONBLOCK);
    }
    if( m_usestderr )
    {
        if( pipe(p_stderr) == -1 )
        {
            VERBOSE(VB_GENERAL, (LOC_ERR + "stderr pipe() failed"));
            m_status = GENERIC_EXIT_NOT_OK;
        }
        else
            fcntl(p_stderr[0], F_SETFL, O_NONBLOCK);
    }

    // set up command args
    const char *command;
    QVector<const char *> _cmdargs;
    if( m_useshell )
    {
        command = "/bin/sh";
        _cmdargs << "sh" << "-c"
                 << m_command.toUtf8().constData() << (char *)0;
    }
    else
    {
        command = m_command.toUtf8().constData();
        _cmdargs << m_command.split('/').last().toUtf8().constData();

        QStringList::const_iterator it = m_args.constBegin();
        while( it != m_args.constEnd() )
        {
                _cmdargs << it->toUtf8().constData();
                it++;
        }
        _cmdargs << (char *)0;
    }
    char **cmdargs = (char **)_cmdargs.data();

    const char *directory;
    directory = m_directory.toUtf8().constData();

    pid_t child = fork();

    if (child < 0)
    {
        /* Fork failed, still in parent */
        VERBOSE(VB_GENERAL, (LOC_ERR + "fork() failed because %1")
                .arg(strerror(errno)));
        m_status = GENERIC_EXIT_NOT_OK;
    }
    else if( child > 0 )
    {
        /* parent */
        m_pid = child;
        m_status = GENERIC_EXIT_RUNNING;

        VERBOSE(VB_GENERAL, QString("Managed child (PID: %1) has started! "
                                            "%2 command=%3, timeout=%4")
                    .arg(m_pid) .arg(m_useshell ? "*" : "")
                    .arg(m_logcmd) .arg(m_timeout));

        /* close unused pipe ends */
        CLOSE(p_stdin[0]);
        CLOSE(p_stdout[1]);
        CLOSE(p_stderr[1]);

        // store the rest
        m_stdpipe[0] = p_stdin[1];
        m_stdpipe[1] = p_stdout[0];
        m_stdpipe[2] = p_stderr[0];
    }
    else if (child == 0)
    {
        /* Child - NOTE: it is not safe to use VERBOSE or QString between the 
         * fork and execv calls in the child.  It causes occasional locking 
         * issues that cause deadlocked child processes. */

        /* handle standard input */
        if( p_stdin[0] >= 0 )
        {
            /* try to attach stdin to input pipe - failure is fatal */
            if( dup2(p_stdin[0], 0) < 0 )
            {
                cerr << locerr 
                     << "Cannot redirect input pipe to standard input: "
                     << strerror(errno) << endl;
                _exit(MYTHSYSTEM__EXIT__PIPE_FAILURE);
            }
        }
        else
        {
            /* try to attach stdin to /dev/null */
            int fd = open("/dev/null", O_RDONLY);
            if( fd >= 0 )
            {
                if( dup2(fd, 0) < 0)
                {
                    cerr << locerr
                         << "Cannot redirect /dev/null to standard input,"
                            "\n\t\t\tfailed to duplicate file descriptor: " 
                         << strerror(errno) << endl;
                }
            }
            else
            {
                cerr << locerr
                     << "Cannot redirect /dev/null to standard input, "
                        "failed to open: "
                     << strerror(errno) << endl;
            }
        }

        /* handle standard output */
        if( p_stdout[1] >= 0 )
        {
            /* try to attach stdout to output pipe - failure is fatal */
            if( dup2(p_stdout[1], 1) < 0)
            {
                cerr << locerr
                     << "Cannot redirect output pipe to standard output: "
                     << strerror(errno) << endl;
                _exit(MYTHSYSTEM__EXIT__PIPE_FAILURE);
            }
        }

        /* handle standard err */
        if( p_stderr[1] >= 0 )
        {
            /* try to attach stderr to error pipe - failure is fatal */
            if( dup2(p_stderr[1], 2) < 0)
            {
                cerr << locerr
                     << "Cannot redirect error pipe to standard error: " 
                     << strerror(errno) << endl;
                _exit(MYTHSYSTEM__EXIT__PIPE_FAILURE);
            }
        }

        /* Close all open file descriptors except stdin/stdout/stderr */
        for( int i = sysconf(_SC_OPEN_MAX) - 1; i > 2; i-- )
            close(i);

        /* set directory */
        if( m_setdirectory )
        {
            chdir(directory);
            if( errno )
            {
                cerr << locerr
                     << "chdir() failed: "
                     << strerror(errno) << endl;
            }
        }

        /* run command */
        execv(command, cmdargs);
        
        if (errno)
        {
            // Can't use VERBOSE due to locking fun.
            cerr << locerr
                 << "execv() failed: "
                 << strerror(errno) << endl;
        }

        /* Failed to exec */
        _exit(MYTHSYSTEM__EXIT__EXECL_ERROR); // this exit is ok
    }

    /* Parent */
    if( m_status != GENERIC_EXIT_RUNNING )
    {
        CLOSE(p_stdin[0]);
        CLOSE(p_stdin[1]);
        CLOSE(p_stdout[0]);
        CLOSE(p_stdout[1]);
        CLOSE(p_stderr[0]);
        CLOSE(p_stderr[1]);
    }
}

uint myth_system(const QString &command, uint flags, uint timeout)
{
    flags |= kMSRunShell;
    MythSystem ms = MythSystem(command, flags);
    ms.Run(timeout);
    uint result = ms.Wait(0);
    return result;
}

extern "C" {
    unsigned int myth_system_c(char *command, uint flags, uint timeout)
    {
        QString cmd(command);
        return myth_system(cmd, flags, timeout);
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
