
// Own header
#include "mythsystem.h"
#include "system-unix.h"

// compat header
#include "compat.h"

// C++/C headers
#include <cerrno>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>  // for kill()
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>

// QT headers
#include <QCoreApplication>
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QString>
#include <QStringList>

// libmythdb headers
#include "mythcorecontext.h"
#include "mythevent.h"
#include "mythverbose.h"
#include "exitcodes.h"

#define CLOSE(x) \
if( (x) >= 0 ) { \
    close((x)); \
    fdLock.lock(); \
    delete fdMap.value((x)); \
    fdMap.remove((x)); \
    fdLock.unlock(); \
    (x) = -1; \
}

typedef struct
{
    MythSystemUnix *ms;
    int             type;
} FDType_t;
typedef QMap<int, FDType_t*> FDMap_t;

/**********************************
 * MythSystemManager method defines
 *********************************/
static MythSystemManager       *manager = NULL;
static MythSystemSignalManager *smanager = NULL;
static MythSystemIOHandler     *readThread = NULL;
static MythSystemIOHandler     *writeThread = NULL;
static MSList_t                 msList;
static QMutex                   listLock;
static FDMap_t                  fdMap;
static QMutex                   fdLock;


MythSystemIOHandler::MythSystemIOHandler(bool read) :
    QThread(), m_pWait(), m_pLock(), m_pMap(PMap_t()),
    m_read(read)
{
}

void MythSystemIOHandler::run(void)
{
    VERBOSE(VB_GENERAL, QString("Starting IO manager (%1)")
        .arg(m_read ? "read" : "write"));

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
                VERBOSE(VB_GENERAL, QString("select(%1, %2) failed: %3")
                            .arg(m_maxfd+1).arg(m_read).arg(strerror(errno)));

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
    {
        buff->buffer().append(m_readbuf, len);

        // Get the corresponding MythSystem instance, and the stdout/stderr
        // type
        fdLock.lock();
        FDType_t *fdType = fdMap.value(fd);
        fdLock.unlock();

        // Emit the data ready signal (1 = stdout, 2 = stderr)
        MythSystemUnix *ms = fdType->ms;
        emit ms->readDataReady(fdType->type);
    }
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
    if (m_read)
    {
        PMap_t::iterator i;
        i = m_pMap.find(fd);
        HandleRead(i.key(), i.value());
    }
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

MythSystemManager::MythSystemManager() : QThread()
{
    m_jumpAbort = false;
}

void MythSystemManager::run(void)
{
    VERBOSE(VB_GENERAL, "Starting process manager");

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

        MythSystemUnix     *ms;
        pid_t               pid;
        int                 status;

        // check for any newly exited processes
        listLock.lock();
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
            msList.append(ms);

            // handle normal exit
            if( WIFEXITED(status) )
            {
                ms->SetStatus(WEXITSTATUS(status));
                VERBOSE(VB_GENERAL, 
                    QString("Managed child (PID: %1) has exited! "
                            "command=%2, status=%3, result=%4")
                    .arg(pid) .arg(ms->GetLogCmd()) .arg(status) 
                    .arg(ms->GetStatus()));
            }

            // handle forced exit
            else if( WIFSIGNALED(status) && 
                     ms->GetStatus() != GENERIC_EXIT_TIMEOUT )
            {
                // Don't override a timed out process which gets killed, but
                // otherwise set the return status appropriately
                int sig = WTERMSIG(status);
                if( sig == 9 )
                    ms->SetStatus( GENERIC_EXIT_ABORTED );
                else if( sig == 11 )
                    ms->SetStatus( GENERIC_EXIT_TERMINATED );
                else
                    ms->SetStatus( GENERIC_EXIT_SIGNALLED );

                VERBOSE(VB_GENERAL, 
                    QString("Managed child (PID: %1) has signalled! "
                            "command=%2, status=%3, result=%4, signal=%5")
                    .arg(pid) .arg(ms->GetLogCmd()) .arg(status) 
                    .arg(ms->GetStatus()) .arg(sig));
            }

            // handle abnormal exit (crash)
            else
            {
                ms->SetStatus( GENERIC_EXIT_NOT_OK );
                VERBOSE(VB_GENERAL, 
                    QString("Managed child (PID: %1) has terminated! "
                            "command=%2, status=%3, result=%4")
                    .arg(pid) .arg(ms->GetLogCmd()) .arg(status) 
                    .arg(ms->GetStatus()));
            }
        }


        // loop through running processes for any that require action
        MSMap_t::iterator   i, next;
        time_t              now = time(NULL);

        m_mapLock.lock();
        m_jumpLock.lock();
        for( i = m_pMap.begin(); i != m_pMap.end(); i = next )
        {
            next = i + 1;
            pid  = i.key();
            ms   = i.value();

            // handle processes beyond marked timeout
            if( ms->m_timeout > 0 && ms->m_timeout < now )
            {
                // issuing KILL signal after TERM failed in a timely manner
                if( ms->GetStatus() == GENERIC_EXIT_TIMEOUT )
                {
                    VERBOSE(VB_GENERAL, 
                        QString("Managed child (PID: %1) timed out"
                                ", issuing KILL signal").arg(pid));
                    // Prevent constant attempts to kill an obstinate child
                    ms->m_timeout = 0;
                    ms->Signal(SIGKILL);
                }

                // issuing TERM signal
                else
                {
                    VERBOSE(VB_GENERAL, 
                        QString("Managed child (PID: %1) timed out"
                                ", issuing TERM signal").arg(pid));
                    ms->SetStatus( GENERIC_EXIT_TIMEOUT );
                    ms->m_timeout = now + 1;
                    ms->Term();
                }
            }

            if ( m_jumpAbort && ms->GetSetting("AbortOnJump") )
                ms->Term();
        }

        m_jumpAbort = false;
        m_jumpLock.unlock();

        m_mapLock.unlock();

        // hold off unlocking until all the way down here to 
        // give the buffer handling a chance to run before
        // being closed down by signal thread
        listLock.unlock();
    }

    // kick to allow them to close themselves cleanly
    readThread->wake();
    writeThread->wake();
}

void MythSystemManager::append(MythSystemUnix *ms)
{
    m_mapLock.lock();
    m_pMap.insert(ms->m_pid, ms);
    m_mapLock.unlock();

    fdLock.lock();
    if( ms->GetSetting("UseStdin") )
        writeThread->insert(ms->m_stdpipe[0], ms->GetBuffer(0));

    if( ms->GetSetting("UseStdout") )
    {
        FDType_t *fdType = new FDType_t;
        fdType->ms = ms;
        fdType->type = 1;
        fdMap.insert( ms->m_stdpipe[1], fdType );
        readThread->insert(ms->m_stdpipe[1], ms->GetBuffer(1));
    }

    if( ms->GetSetting("UseStderr") )
    {
        FDType_t *fdType = new FDType_t;
        fdType->ms = ms;
        fdType->type = 2;
        fdMap.insert( ms->m_stdpipe[2], fdType );
        readThread->insert(ms->m_stdpipe[2], ms->GetBuffer(2));
    }
    fdLock.unlock();
}

void MythSystemManager::jumpAbort(void)
{
    m_jumpLock.lock();
    m_jumpAbort = true;
    m_jumpLock.unlock();
}

// spawn separate thread for signals to prevent manager
// thread from blocking in some slot
MythSystemSignalManager::MythSystemSignalManager() : QThread()
{
}

void MythSystemSignalManager::run(void)
{
    VERBOSE(VB_GENERAL, "Starting process signal handler");
    while( gCoreContext )
    {
        usleep(50000); // sleep 50ms
        while( gCoreContext )
        {
            // handle cleanup and signalling for closed processes
            listLock.lock();
            if( msList.isEmpty() )
            {
                listLock.unlock();
                break;
            }
            MythSystemUnix *ms = msList.takeFirst();
            listLock.unlock();

            ms->m_parent->HandlePostRun();

            if (ms->m_stdpipe[0] > 0)
                writeThread->remove(ms->m_stdpipe[0]);
            CLOSE(ms->m_stdpipe[0]);

            if (ms->m_stdpipe[1] > 0)
                readThread->remove(ms->m_stdpipe[1]);
            CLOSE(ms->m_stdpipe[1]);

            if (ms->m_stdpipe[2] > 0)
                readThread->remove(ms->m_stdpipe[2]);
            CLOSE(ms->m_stdpipe[2]);

            if( ms->GetStatus() == GENERIC_EXIT_OK )
                emit ms->finished();
            else
                emit ms->error(ms->GetStatus());

            ms->disconnect();

            ms->Unlock();

            if( ms->m_parent->doAutoCleanup() )
                delete ms;
        }
    }
}

/*******************************
 * MythSystem method defines
 ******************************/

MythSystemUnix::MythSystemUnix(MythSystem *parent)
{
    m_parent = parent;

    connect( this, SIGNAL(started()), m_parent, SIGNAL(started()) );
    connect( this, SIGNAL(finished()), m_parent, SIGNAL(finished()) );
    connect( this, SIGNAL(error(uint)), m_parent, SIGNAL(error(uint)) );
    connect( this, SIGNAL(readDataReady(int)), m_parent, SIGNAL(readDataReady(int)) );

    // Start the threads if they haven't been started yet.
    if( manager == NULL )
    {
        manager = new MythSystemManager;
        manager->start();
    }

    if( smanager == NULL )
    {
        smanager = new MythSystemSignalManager;
        smanager->start();
    }

    if( readThread == NULL )
    {
        readThread = new MythSystemIOHandler(true);
        readThread->start();
    }

    if( writeThread == NULL )
    {
        writeThread = new MythSystemIOHandler(false);
        writeThread->start();
    }
}

// QBuffers may also need freeing
MythSystemUnix::~MythSystemUnix(void)
{
}


void MythSystemUnix::Term(bool force)
{
    if( (GetStatus() != GENERIC_EXIT_RUNNING) || (m_pid <= 0) )
        return;

    Signal(SIGTERM);
    if( force )
    {
        // send KILL if it does not exit within one second
        if( m_parent->Wait(1) == GENERIC_EXIT_RUNNING )
            Signal(SIGKILL);
    }
}

void MythSystemUnix::Signal( int sig )
{
    if( (GetStatus() != GENERIC_EXIT_RUNNING) || (m_pid <= 0) )
        return;
    VERBOSE(VB_GENERAL, QString("Child PID %1 killed with %2")
                    .arg(m_pid).arg(strsignal(sig)));
    kill((GetSetting("SetPGID") ? -m_pid : m_pid), sig);
}

#define MAX_BUFLEN 1024
void MythSystemUnix::Fork(time_t timeout)
{
    QString LOC_ERR = QString("myth_system('%1'): Error: ").arg(GetLogCmd());

    // For use in the child
    char locerr[MAX_BUFLEN];
    strncpy(locerr, (const char *)LOC_ERR.toUtf8().constData(), MAX_BUFLEN);
    locerr[MAX_BUFLEN-1] = '\0';

    VERBOSE(VB_GENERAL, QString("Launching: %1").arg(GetLogCmd()));

    GetBuffer(0)->setBuffer(0);
    GetBuffer(1)->setBuffer(0);
    GetBuffer(2)->setBuffer(0);

    int p_stdin[]  = {-1,-1};
    int p_stdout[] = {-1,-1};
    int p_stderr[] = {-1,-1};

    /* set up pipes */
    if( GetSetting("UseStdin") )
    {
        if( pipe(p_stdin) == -1 )
        {
            VERBOSE(VB_GENERAL, (LOC_ERR + "stdin pipe() failed"));
            SetStatus( GENERIC_EXIT_NOT_OK );
        }
        else
            fcntl(p_stdin[1], F_SETFL, O_NONBLOCK);
    }
    if( GetSetting("UseStdout") )
    {
        if( pipe(p_stdout) == -1 )
        {
            VERBOSE(VB_GENERAL, (LOC_ERR + "stdout pipe() failed"));
            SetStatus( GENERIC_EXIT_NOT_OK );
        }
        else
            fcntl(p_stdout[0], F_SETFL, O_NONBLOCK);
    }
    if( GetSetting("UseStderr") )
    {
        if( pipe(p_stderr) == -1 )
        {
            VERBOSE(VB_GENERAL, (LOC_ERR + "stderr pipe() failed"));
            SetStatus( GENERIC_EXIT_NOT_OK );
        }
        else
            fcntl(p_stderr[0], F_SETFL, O_NONBLOCK);
    }

    // set up command args
    if (GetSetting("UseShell"))
    {
        QStringList args = QStringList("-c");
        args << GetCommand() + " " + GetArgs().join(" ");
        SetArgs( args );
        QString cmd = "/bin/sh";
        SetCommand( cmd );
    }
    QStringList args = GetArgs();
    args.prepend(GetCommand().split('/').last());
    SetArgs( args );

    QByteArray cmdUTF8 = GetCommand().toUtf8();
    const char *command = strdup(cmdUTF8.constData());

    char **cmdargs = (char **)malloc((args.size() + 1) * sizeof(char *));
    int i;
    QStringList::const_iterator it;

    for( i = 0, it = args.constBegin(); it != args.constEnd(); it++, i++ )
    {
        cmdargs[i] = strdup( it->toUtf8().constData() );
    }
    cmdargs[i] = NULL;

    const char *directory = NULL;
    QString dir = GetDirectory();
    if (GetSetting("SetDirectory") && !dir.isEmpty())
        directory = strdup(dir.toUtf8().constData());

    pid_t child = fork();

    if (child < 0)
    {
        /* Fork failed, still in parent */
        VERBOSE(VB_GENERAL, (LOC_ERR + "fork() failed because %1")
                .arg(strerror(errno)));
        SetStatus( GENERIC_EXIT_NOT_OK );
    }
    else if( child > 0 )
    {
        /* parent */
        m_pid = child;
        SetStatus( GENERIC_EXIT_RUNNING );

        VERBOSE(VB_GENERAL, QString("Managed child (PID: %1) has started! "
                                            "%2%3 command=%4, timeout=%5")
                    .arg(m_pid) .arg(GetSetting("UseShell") ? "*" : "")
                    .arg(GetSetting("RunInBackground") ? "&" : "")
                    .arg(GetLogCmd()) .arg(m_timeout));

        m_timeout = timeout;
        if( timeout )
            m_timeout += time(NULL);

        /* close unused pipe ends */
        CLOSE(p_stdin[0]);
        CLOSE(p_stdout[1]);
        CLOSE(p_stderr[1]);

        // store the rest
        m_stdpipe[0] = p_stdin[1];
        m_stdpipe[1] = p_stdout[0];
        m_stdpipe[2] = p_stderr[0];

        // clean up the memory use
        if( command )
            free((void *)command);

        if( directory )
            free((void *)directory);

        if( cmdargs )
        {
            for (i = 0; cmdargs[i]; i++)
                free( cmdargs[i] );
            free( cmdargs );
        }
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
        if( directory && chdir(directory) < 0 )
        {
            cerr << locerr
                 << "chdir() failed: "
                 << strerror(errno) << endl;
        }

        /* Set the process group id to be the same as the pid of this child
         * process.  This ensures that any subprocesses launched by this
         * process can be killed along with the process itself. */ 
        if (GetSetting("SetPGID") && setpgid(0,0) < 0 ) 
        {
            cerr << locerr
                 << "setpgid() failed: "
                 << strerror(errno) << endl;
        }

        /* run command */
        if( execv(command, cmdargs) < 0 )
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
    if( GetStatus() != GENERIC_EXIT_RUNNING )
    {
        CLOSE(p_stdin[0]);
        CLOSE(p_stdin[1]);
        CLOSE(p_stdout[0]);
        CLOSE(p_stdout[1]);
        CLOSE(p_stderr[0]);
        CLOSE(p_stderr[1]);
    }
}

void MythSystemUnix::Manage(void)
{
    if( manager == NULL )
    {
        manager = new MythSystemManager;
        manager->start();
    }
    manager->append(this);
}

void MythSystemUnix::JumpAbort(void)
{
    if( manager == NULL )
    {
        manager = new MythSystemManager;
        manager->start();
    }
    manager->jumpAbort();
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
