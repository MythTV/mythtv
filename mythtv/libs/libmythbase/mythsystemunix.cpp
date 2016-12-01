
// Own header
#include "mythsystemlegacy.h"
#include "mythsystemunix.h"
#include "mythmiscutil.h"

// compat header
#include "compat.h"

// C++/C headers
#include <cerrno>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>  // for kill()
#include <string.h> // for strerror()
#include <sys/select.h>
#include <sys/wait.h>
#include <iostream> // for cerr()
using namespace std; // for most of the above

// QT headers
#include <QCoreApplication>
#include <QMutex>
#include <QMap>
#include <QString>
#include <QStringList>

// libmythbase headers
#include "mythcorecontext.h"
#include "mythevent.h"
#include "exitcodes.h"
#include "mythlogging.h"

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
    MythSystemLegacyUnix *ms;
    int             type;
} FDType_t;
typedef QMap<int, FDType_t*> FDMap_t;

/**********************************
 * MythSystemLegacyManager method defines
 *********************************/
static bool                     run_system = true;
static MythSystemLegacyManager       *manager = NULL;
static MythSystemLegacySignalManager *smanager = NULL;
static MythSystemLegacyIOHandler     *readThread = NULL;
static MythSystemLegacyIOHandler     *writeThread = NULL;
static MSList_t                 msList;
static QMutex                   listLock;
static FDMap_t                  fdMap;
static QMutex                   fdLock;

void ShutdownMythSystemLegacy(void)
{
    run_system = false;
    if (manager)
        manager->wait();
    if (smanager)
        smanager->wait();
    if (readThread)
        readThread->wait();
    if (writeThread)
        writeThread->wait();
}

MythSystemLegacyIOHandler::MythSystemLegacyIOHandler(bool read) :
    MThread(QString("SystemIOHandler%1").arg(read ? "R" : "W")),
    m_pWaitLock(), m_pWait(), m_pLock(), m_pMap(PMap_t()), m_maxfd(-1),
    m_read(read)
{
    FD_ZERO(&m_fds);
    m_readbuf[0] = '\0';
}

void MythSystemLegacyIOHandler::run(void)
{
    RunProlog();
    LOG(VB_GENERAL, LOG_INFO, QString("Starting IO manager (%1)")
                .arg(m_read ? "read" : "write"));

    m_pLock.lock();
    BuildFDs();
    m_pLock.unlock();

    while( run_system )
    {
        {
            QMutexLocker locker(&m_pWaitLock);
            m_pWait.wait(&m_pWaitLock);
        }

        while( run_system )
        {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 10*1000*1000;  // 10ms
            nanosleep(&ts, NULL); // ~100x per second, for ~3MBps throughput
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
                LOG(VB_SYSTEM, LOG_ERR,
                    QString("MythSystemLegacyIOHandler: select(%1, %2) failed: %3")
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

    RunEpilog();
}

void MythSystemLegacyIOHandler::HandleRead(int fd, QBuffer *buff)
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

        // Get the corresponding MythSystemLegacy instance, and the stdout/stderr
        // type
        fdLock.lock();
        FDType_t *fdType = fdMap.value(fd);
        fdLock.unlock();

        // Emit the data ready signal (1 = stdout, 2 = stderr)
        MythSystemLegacyUnix *ms = fdType->ms;
        emit ms->readDataReady(fdType->type);
    }
}

void MythSystemLegacyIOHandler::HandleWrite(int fd, QBuffer *buff)
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

void MythSystemLegacyIOHandler::insert(int fd, QBuffer *buff)
{
    m_pLock.lock();
    m_pMap.insert(fd, buff);
    BuildFDs();
    m_pLock.unlock();
    wake();
}

void MythSystemLegacyIOHandler::Wait(int fd)
{
    QMutexLocker locker(&m_pLock);
    while (m_pMap.contains(fd))
    {
        locker.unlock();
        usleep(10 * 1000);
        locker.relock();
    }
}

void MythSystemLegacyIOHandler::remove(int fd)
{
    m_pLock.lock();
    if (m_read)
    {
        PMap_t::iterator i;
        i = m_pMap.find(fd);
        if ( i != m_pMap.end() )
            HandleRead(i.key(), i.value());
    }
    m_pMap.remove(fd);
    BuildFDs();
    m_pLock.unlock();
}

void MythSystemLegacyIOHandler::wake()
{
    QMutexLocker locker(&m_pWaitLock);
    m_pWait.wakeAll();
}

void MythSystemLegacyIOHandler::BuildFDs()
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

MythSystemLegacyManager::MythSystemLegacyManager() : MThread("SystemManager")
{
    m_jumpAbort = false;
}

void MythSystemLegacyManager::run(void)
{
    RunProlog();
    LOG(VB_GENERAL, LOG_INFO, "Starting process manager");

    // run_system is set to NULL during shutdown, and we need this thread to
    // exit during shutdown.
    while( run_system )
    {
        m_mapLock.lock();
        m_wait.wait(&m_mapLock, m_pMap.isEmpty() ? 100 : 20);

        // check for any running processes

        if( m_pMap.isEmpty() )
        {
            m_mapLock.unlock();
            continue;
        }
        m_mapLock.unlock();

        MythSystemLegacyUnix     *ms;
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
                LOG(VB_SYSTEM, LOG_INFO,
                    QString("Unmanaged child (PID: %1) has exited!") .arg(pid));
                m_mapLock.unlock();
                continue;
            }

            // pop exited process off managed list, add to cleanup list
            ms = m_pMap.take(pid);
            m_mapLock.unlock();

            // Occasionally, the caller has deleted the structure from under
            // our feet.  If so, just log and move on.
            if (!ms || !ms->m_parent)
            {
                LOG(VB_SYSTEM, LOG_ERR,
                    QString("Structure for child PID %1 already deleted!")
                    .arg(pid));
                if (ms)
                    ms->DecrRef();
                continue;
            }

            msList.append(ms);

            // Deal with (primarily) Ubuntu which seems to consistently be
            // screwing up and reporting the signalled case as an exit.  This
            // workaround will limit the valid exit value to 0 - 127.  As all
            // of our return values match that (other than the occasional 255)
            // this shouldn't cause any issues
            if (ms->m_parent->onlyLowExitVal() && WIFEXITED(status) &&
                WEXITSTATUS(status) != 255 && WEXITSTATUS(status) & 0x80)
            {
                // Just byte swap the status and it seems to be correctly done
                uint16_t oldstatus = status;
                status = ((status & 0x00FF) << 8) | ((status & 0xFF00) >> 8);
                LOG(VB_SYSTEM, LOG_INFO,
                    QString("Odd return value: swapping from %1 to %2")
                    .arg(oldstatus) .arg(status));
            }

            // handle normal exit
            if( WIFEXITED(status) )
            {
                ms->SetStatus(WEXITSTATUS(status));
                LOG(VB_SYSTEM, LOG_INFO,
                    QString("Managed child (PID: %1) has exited! "
                            "command=%2, status=%3, result=%4")
                    .arg(pid) .arg(ms->GetLogCmd()) .arg(status)
                    .arg(ms->GetStatus()));
            }

            // handle forced exit
            else if( WIFSIGNALED(status) )
            {
                // Don't override a timed out process which gets killed, but
                // otherwise set the return status appropriately
                if (ms->GetStatus() != GENERIC_EXIT_TIMEOUT)
                    ms->SetStatus( GENERIC_EXIT_KILLED );

                int sig = WTERMSIG(status);
                LOG(VB_SYSTEM, LOG_INFO,
                    QString("Managed child (PID: %1) has signalled! "
                            "command=%2, status=%3, result=%4, signal=%5")
                    .arg(pid) .arg(ms->GetLogCmd()) .arg(status)
                    .arg(ms->GetStatus()) .arg(sig));
            }

            // handle abnormal exit (crash)
            else
            {
                ms->SetStatus( GENERIC_EXIT_NOT_OK );
                LOG(VB_SYSTEM, LOG_ERR,
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
            if (!ms)
                continue;

            // handle processes beyond marked timeout
            if( ms->m_timeout > 0 && ms->m_timeout < now )
            {
                // issuing KILL signal after TERM failed in a timely manner
                if( ms->GetStatus() == GENERIC_EXIT_TIMEOUT )
                {
                    LOG(VB_SYSTEM, LOG_INFO,
                        QString("Managed child (PID: %1) timed out"
                                ", issuing KILL signal").arg(pid));
                    // Prevent constant attempts to kill an obstinate child
                    ms->m_timeout = 0;
                    ms->Signal(SIGKILL);
                }

                // issuing TERM signal
                else
                {
                    LOG(VB_SYSTEM, LOG_INFO,
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
    if (readThread)
        readThread->wake();
    if (writeThread)
        writeThread->wake();

    RunEpilog();
}

void MythSystemLegacyManager::append(MythSystemLegacyUnix *ms)
{
    m_mapLock.lock();
    ms->IncrRef();
    m_pMap.insert(ms->m_pid, ms);
    m_wait.wakeAll();
    m_mapLock.unlock();

    if (ms->m_stdpipe[0] >= 0)
    {
        QByteArray ba = ms->GetBuffer(0)->data();
        QBuffer wtb(&ba);
        wtb.open(QIODevice::ReadOnly);
        writeThread->insert(ms->m_stdpipe[0], &wtb);
        writeThread->Wait(ms->m_stdpipe[0]);
        writeThread->remove(ms->m_stdpipe[0]);
        CLOSE(ms->m_stdpipe[0]);
    }

    if( ms->GetSetting("UseStdout") )
    {
        FDType_t *fdType = new FDType_t;
        fdType->ms = ms;
        fdType->type = 1;
        fdLock.lock();
        fdMap.insert( ms->m_stdpipe[1], fdType );
        fdLock.unlock();
        readThread->insert(ms->m_stdpipe[1], ms->GetBuffer(1));
    }

    if( ms->GetSetting("UseStderr") )
    {
        FDType_t *fdType = new FDType_t;
        fdType->ms = ms;
        fdType->type = 2;
        fdLock.lock();
        fdMap.insert( ms->m_stdpipe[2], fdType );
        fdLock.unlock();
        readThread->insert(ms->m_stdpipe[2], ms->GetBuffer(2));
    }
}

void MythSystemLegacyManager::jumpAbort(void)
{
    m_jumpLock.lock();
    m_jumpAbort = true;
    m_jumpLock.unlock();
}

// spawn separate thread for signals to prevent manager
// thread from blocking in some slot
MythSystemLegacySignalManager::MythSystemLegacySignalManager() :
    MThread("SystemSignalManager")
{
}

void MythSystemLegacySignalManager::run(void)
{
    RunProlog();
    LOG(VB_GENERAL, LOG_INFO, "Starting process signal handler");
    while (run_system)
    {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 50 * 1000 * 1000; // 50ms
        nanosleep(&ts, NULL); // sleep 50ms

        while (run_system)
        {
            // handle cleanup and signalling for closed processes
            listLock.lock();
            if (msList.isEmpty())
            {
                listLock.unlock();
                break;
            }
            MythSystemLegacyUnix *ms = msList.takeFirst();
            listLock.unlock();

            // This can happen if it has been deleted already
            if (!ms)
                continue;

            if (ms->m_parent)
            {
                ms->m_parent->HandlePostRun();
            }

            if (ms->m_stdpipe[0] >= 0)
                writeThread->remove(ms->m_stdpipe[0]);
            CLOSE(ms->m_stdpipe[0]);

            if (ms->m_stdpipe[1] >= 0)
                readThread->remove(ms->m_stdpipe[1]);
            CLOSE(ms->m_stdpipe[1]);

            if (ms->m_stdpipe[2] >= 0)
                readThread->remove(ms->m_stdpipe[2]);
            CLOSE(ms->m_stdpipe[2]);

            if (ms->m_parent)
            {
                if (ms->GetStatus() == GENERIC_EXIT_OK)
                    emit ms->finished();
                else
                    emit ms->error(ms->GetStatus());

                ms->disconnect();
                ms->Unlock();
            }

            ms->DecrRef();
        }
    }
    RunEpilog();
}

/*******************************
 * MythSystemLegacy method defines
 ******************************/

MythSystemLegacyUnix::MythSystemLegacyUnix(MythSystemLegacy *parent) :
    MythSystemLegacyPrivate("MythSystemLegacyUnix"),
    m_pid(0), m_timeout(0)
{
    m_parent = parent;

    m_stdpipe[0] = -1;
    m_stdpipe[1] = -1;
    m_stdpipe[2] = -1;

    connect(this, SIGNAL(started()), m_parent, SIGNAL(started()));
    connect(this, SIGNAL(finished()), m_parent, SIGNAL(finished()));
    connect(this, SIGNAL(error(uint)), m_parent, SIGNAL(error(uint)));
    connect(this, SIGNAL(readDataReady(int)),
            m_parent, SIGNAL(readDataReady(int)));

    // Start the threads if they haven't been started yet.
    if( manager == NULL )
    {
        manager = new MythSystemLegacyManager;
        manager->start();
    }

    if( smanager == NULL )
    {
        smanager = new MythSystemLegacySignalManager;
        smanager->start();
    }

    if( readThread == NULL )
    {
        readThread = new MythSystemLegacyIOHandler(true);
        readThread->start();
    }

    if( writeThread == NULL )
    {
        writeThread = new MythSystemLegacyIOHandler(false);
        writeThread->start();
    }
}

MythSystemLegacyUnix::~MythSystemLegacyUnix(void)
{
}

bool MythSystemLegacyUnix::ParseShell(const QString &cmd, QString &abscmd,
                                QStringList &args)
{
    QList<QChar> whitespace; whitespace << ' ' << '\t' << '\n' << '\r';
    QList<QChar> whitechr; whitechr << 't' << 'n' << 'r';
    QChar quote = '"',
      hardquote = '\'',
         escape = '\\';
    bool quoted = false,
     hardquoted = false,
        escaped = false;

    QString tmp;
    QString::const_iterator i = cmd.begin();
    while (i != cmd.end())
    {
        if (quoted || hardquoted)
        {
            if (escaped)
            {
                if ((quote == *i) || (escape == *i) ||
                            whitespace.contains(*i))
                    // pass through escape (\), quote ("), and any whitespace
                    tmp += *i;
                else if (whitechr.contains(*i))
                    // process whitespace escape code, and pass character
                    tmp += whitespace[whitechr.indexOf(*i)+1];
                else
                    // unhandled escape code, abort
                    return false;

                escaped = false;
            }

            else if (*i == escape)
            {
                if (hardquoted)
                    // hard quotes (') pass everything
                    tmp += *i;
                else
                    // otherwise, mark escaped to handle next character
                    escaped = true;
            }

            else if ((quoted & (*i == quote)) ||
                            (hardquoted && (*i == hardquote)))
                // end of quoted sequence
                quoted = hardquoted = false;

            else
                // pass through character
                tmp += *i;
        }

        else if (escaped)
        {
            if ((*i == quote) || (*i == hardquote) || (*i == escape) ||
                    whitespace.contains(*i))
                // pass through special characters
                tmp += *i;
            else if (whitechr.contains(*i))
                // process whitespace escape code, and pass character
                tmp += whitespace[whitechr.indexOf(*i)+1];
            else
                // unhandled escape code, abort
                return false;

            escaped = false;
        }

        // handle quotes and escape characters
        else if (quote == *i)
            quoted = true;
        else if (hardquote == *i)
            hardquoted = true;
        else if (escape == *i)
            escaped = true;

        // handle whitespace characters
        else if (whitespace.contains(*i) && !tmp.isEmpty())
        {
            args << tmp;
            tmp.clear();
        }

        else
            // pass everything else
            tmp += *i;

        // step forward to next character
        ++i;
    }

    if (quoted || hardquoted || escaped)
        // command not terminated cleanly
        return false;

    if (!tmp.isEmpty())
        // collect last argument
        args << tmp;

    if (args.isEmpty())
        // this shouldnt happen
        return false;

    // grab the first argument to use as the command
    abscmd = args.takeFirst();
    if (!abscmd.startsWith('/'))
    {
        // search for absolute path
        QStringList path = QString(getenv("PATH")).split(':');
        QStringList::const_iterator i = path.begin();
        for (; i != path.end(); ++i)
        {
            QFile file(QString("%1/%2").arg(*i).arg(abscmd));
            if (file.exists())
            {
                abscmd = file.fileName();
                break;
            }
        }
    }

    return true;
}

void MythSystemLegacyUnix::Term(bool force)
{
    int status = GetStatus();
    if( (status != GENERIC_EXIT_RUNNING && status != GENERIC_EXIT_TIMEOUT) ||
        (m_pid <= 0) )
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Terminate skipped. Status: %1")
            .arg(status));
        return;
    }

    Signal(SIGTERM);
    if( force )
    {
        // send KILL if it does not exit within one second
        if( m_parent->Wait(1) == GENERIC_EXIT_RUNNING )
            Signal(SIGKILL);
    }
}

void MythSystemLegacyUnix::Signal( int sig )
{
    int status = GetStatus();
    if( (status != GENERIC_EXIT_RUNNING && status != GENERIC_EXIT_TIMEOUT) ||
        (m_pid <= 0) )
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Signal skipped. Status: %1")
            .arg(status));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Child PID %1 killed with %2")
                    .arg(m_pid).arg(strsignal(sig)));
    kill(m_pid, sig);
}

#define MAX_BUFLEN 1024
void MythSystemLegacyUnix::Fork(time_t timeout)
{
    QString LOC_ERR = QString("myth_system('%1'): Error: ").arg(GetLogCmd());

    // For use in the child
    char locerr[MAX_BUFLEN];
    strncpy(locerr, (const char *)LOC_ERR.toUtf8().constData(), MAX_BUFLEN);
    locerr[MAX_BUFLEN-1] = '\0';

    LOG(VB_SYSTEM, LOG_DEBUG, QString("Launching: %1").arg(GetLogCmd()));

    int p_stdin[]  = {-1,-1};
    int p_stdout[] = {-1,-1};
    int p_stderr[] = {-1,-1};

    /* set up pipes */
    if( GetSetting("UseStdin") )
    {
        if( pipe(p_stdin) == -1 )
        {
            LOG(VB_SYSTEM, LOG_ERR, LOC_ERR + "stdin pipe() failed");
            SetStatus( GENERIC_EXIT_NOT_OK );
        }
        else
        {
            int flags = fcntl(p_stdin[1], F_GETFL, 0);
            if (flags == -1)
            {
                LOG(VB_SYSTEM, LOG_ERR, LOC_ERR +
                    "fcntl on stdin pipe getting flags failed" +
                    ENO);
            }
            else
            {
                flags |= O_NONBLOCK;
                if(fcntl(p_stdin[1], F_SETFL, flags) == -1)
                {
                    LOG(VB_SYSTEM, LOG_ERR, LOC_ERR +
                        "fcntl on stdin pipe setting non-blocking failed" +
                        ENO);
                }
            }
        }
    }
    if( GetSetting("UseStdout") )
    {
        if( pipe(p_stdout) == -1 )
        {
            LOG(VB_SYSTEM, LOG_ERR, LOC_ERR + "stdout pipe() failed");
            SetStatus( GENERIC_EXIT_NOT_OK );
        }
        else
        {
            int flags = fcntl(p_stdout[0], F_GETFL, 0);
            if (flags == -1)
            {
                LOG(VB_SYSTEM, LOG_ERR, LOC_ERR +
                    "fcntl on stdout pipe getting flags failed" +
                    ENO);
            }
            else
            {
                flags |= O_NONBLOCK;
                if(fcntl(p_stdout[0], F_SETFL, flags) == -1)
                {
                    LOG(VB_SYSTEM, LOG_ERR, LOC_ERR +
                        "fcntl on stdout pipe setting non-blocking failed" +
                        ENO);
                }
            }
        }
    }
    if( GetSetting("UseStderr") )
    {
        if( pipe(p_stderr) == -1 )
        {
            LOG(VB_SYSTEM, LOG_ERR, LOC_ERR + "stderr pipe() failed");
            SetStatus( GENERIC_EXIT_NOT_OK );
        }
        else
        {
            int flags = fcntl(p_stderr[0], F_GETFL, 0);
            if (flags == -1)
            {
                LOG(VB_SYSTEM, LOG_ERR, LOC_ERR +
                    "fcntl on stderr pipe getting flags failed" +
                    ENO);
            }
            else
            {
                flags |= O_NONBLOCK;
                if(fcntl(p_stderr[0], F_SETFL, flags) == -1)
                {
                    LOG(VB_SYSTEM, LOG_ERR, LOC_ERR +
                        "fcntl on stderr pipe setting non-blocking failed" +
                        ENO);
                }
            }
        }
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
    char *command = strdup(cmdUTF8.constData());

    char **cmdargs = (char **)malloc((args.size() + 1) * sizeof(char *));
    int i;
    QStringList::const_iterator it;

    if (cmdargs)
    {
        for (i = 0, it = args.constBegin(); it != args.constEnd(); ++it)
        {
            cmdargs[i++] = strdup(it->toUtf8().constData());
        }
        cmdargs[i] = (char *)NULL;
    }
    else
        LOG(VB_GENERAL, LOG_CRIT, LOC_ERR +
                        "Failed to allocate memory for cmdargs " +
                        ENO);

    char *directory = NULL;
    QString dir = GetDirectory();
    if (GetSetting("SetDirectory") && !dir.isEmpty())
        directory = strdup(dir.toUtf8().constData());

    int niceval = m_parent->GetNice();
    int ioprioval = m_parent->GetIOPrio();

    /* Do this before forking in case the child miserably fails */
    m_timeout = timeout;
    if( timeout )
        m_timeout += time(NULL);

    listLock.lock();
    pid_t child = fork();

    if (child < 0)
    {
        /* Fork failed, still in parent */
        LOG(VB_SYSTEM, LOG_ERR, "fork() failed: " + ENO);
        SetStatus( GENERIC_EXIT_NOT_OK );
        listLock.unlock();
    }
    else if( child > 0 )
    {
        /* parent */
        m_pid = child;
        SetStatus( GENERIC_EXIT_RUNNING );

        LOG(VB_SYSTEM, LOG_INFO,
                    QString("Managed child (PID: %1) has started! "
                            "%2%3 command=%4, timeout=%5")
                        .arg(m_pid) .arg(GetSetting("UseShell") ? "*" : "")
                        .arg(GetSetting("RunInBackground") ? "&" : "")
                        .arg(GetLogCmd()) .arg(timeout));

        /* close unused pipe ends */
        if (p_stdin[0] >= 0)
            close(p_stdin[0]);
        if (p_stdout[1] >= 0)
            close(p_stdout[1]);
        if (p_stderr[1] >= 0)
            close(p_stderr[1]);

        // store the rest
        m_stdpipe[0] = p_stdin[1];
        m_stdpipe[1] = p_stdout[0];
        m_stdpipe[2] = p_stderr[0];

        if( manager == NULL )
        {
            manager = new MythSystemLegacyManager;
            manager->start();
        }
        manager->append(this);

        listLock.unlock();
    }
    else if (child == 0)
    {
        /* Child - NOTE: it is not safe to use LOG or QString between the
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
                _exit(GENERIC_EXIT_PIPE_FAILURE);
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
                if (fd != 0)    // if fd was zero, do not close
                {
                    if (close(fd) < 0)
                    {
                        cerr << locerr
                             << "Unable to close stdin redirect /dev/null: "
                             << strerror(errno) << endl;
                    }
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
                _exit(GENERIC_EXIT_PIPE_FAILURE);
            }
        }
        else
        {
            /* We aren't sucking this down, redirect stdout to /dev/null */
            int fd = open("/dev/null", O_WRONLY);
            if( fd >= 0 )
            {
                if( dup2(fd, 1) < 0)
                {
                    cerr << locerr
                         << "Cannot redirect standard output to /dev/null,"
                            "\n\t\t\tfailed to duplicate file descriptor: "
                         << strerror(errno) << endl;
                }
                if (fd != 1)    // if fd was one, do not close
                {
                   if (close(fd) < 0)
                   {
                       cerr << locerr
                            << "Unable to close stdout redirect /dev/null: "
                            << strerror(errno) << endl;
                   }
                }
            }
            else
            {
                cerr << locerr
                     << "Cannot redirect standard output to /dev/null, "
                        "failed to open: "
                     << strerror(errno) << endl;
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
                _exit(GENERIC_EXIT_PIPE_FAILURE);
            }
        }
        else
        {
            /* We aren't sucking this down, redirect stderr to /dev/null */
            int fd = open("/dev/null", O_WRONLY);
            if( fd >= 0 )
            {
                if( dup2(fd, 2) < 0)
                {
                    cerr << locerr
                         << "Cannot redirect standard error to /dev/null,"
                            "\n\t\t\tfailed to duplicate file descriptor: "
                         << strerror(errno) << endl;
                }
                if (fd != 2)    // if fd was two, do not close
                {
                   if (close(fd) < 0)
                   {
                       cerr << locerr
                            << "Unable to close stderr redirect /dev/null: "
                            << strerror(errno) << endl;
                   }
                }
            }
            else
            {
                cerr << locerr
                     << "Cannot redirect standard error to /dev/null, "
                        "failed to open: "
                     << strerror(errno) << endl;
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

        /* Set nice and ioprio values if non-default */
        if (niceval)
            myth_nice(niceval);
        if (ioprioval)
            myth_ioprio(ioprioval);

        /* run command */
        if( execv(command, cmdargs) < 0 )
        {
            // Can't use LOG due to locking fun.
            cerr << locerr
                 << "execv() failed: "
                 << strerror(errno) << endl;
        }

        /* Failed to exec */
        _exit(GENERIC_EXIT_DAEMONIZING_ERROR); // this exit is ok
    }

    /* Parent */

    // clean up the memory use
    free(command);

    free(directory);

    if( cmdargs )
    {
        for (i = 0; cmdargs[i]; i++)
            free( cmdargs[i] );
        free( cmdargs );
    }

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

void MythSystemLegacyUnix::Manage(void)
{
}

void MythSystemLegacyUnix::JumpAbort(void)
{
    if( manager == NULL )
    {
        manager = new MythSystemLegacyManager;
        manager->start();
    }
    manager->jumpAbort();
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
