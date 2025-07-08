
// compat header
#include "compat.h"

// Own header
#include "mythsystemlegacy.h"
#include "mythsystemwindows.h"

// C++/C headers
#include <cerrno>
#include <csignal>  // for kill()
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

// QT headers
#include <QCoreApplication>
#include <QMutex>
#include <QMap>
#include <QString>
#include <QStringList>

// libmythbase headers
#include "mythlogging.h"
#include "mythevent.h"
#include "exitcodes.h"

// Windows headers
#include <windows.h>
#include <tchar.h>

#define CLOSE(x) \
if( (x) ) { \
    CloseHandle((x)); \
    fdLock.lock(); \
    delete fdMap.value((x)); \
    fdMap.remove((x)); \
    fdLock.unlock(); \
    (x) = nullptr; \
}

struct FDType_t
{
    MythSystemLegacyWindows *ms;
    int                type;
};
using FDMap_t = QMap<HANDLE, FDType_t*>;

/**********************************
 * MythSystemLegacyManager method defines
 *********************************/
static bool                     run_system = true;
static MythSystemLegacyManager       *manager = nullptr;
static MythSystemLegacySignalManager *smanager = nullptr;
static MythSystemLegacyIOHandler     *readThread = nullptr;
static MythSystemLegacyIOHandler     *writeThread = nullptr;
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
    m_pWaitLock(), m_pWait(), m_pLock(), m_pMap(PMap_t()),
    m_read(read)
{
    m_readbuf[0] = '\0';
}

void MythSystemLegacyIOHandler::run(void)
{
    RunProlog();

    LOG(VB_GENERAL, LOG_INFO, QString("Starting IO manager (%1)")
        .arg(m_read ? "read" : "write"));

    while( run_system )
    {
        {
            QMutexLocker locker(&m_pWaitLock);
            m_pWait.wait(&m_pWaitLock);
        }

        while( run_system )
        {
            usleep(10ms); // ~100x per second, for ~3MBps throughput
            m_pLock.lock();
            if( m_pMap.isEmpty() )
            {
                m_pLock.unlock();
                break;
            }

            bool datafound = true;
            m_pLock.unlock();

            while ( datafound && run_system )
            {
                m_pLock.lock();

                datafound = false;
                PMap_t::iterator i, next;
                for( i = m_pMap.begin(); i != m_pMap.end(); i = next )
                {
                    next = i + 1;

                    if( m_read )
                        datafound |= HandleRead(i.key(), i.value());
                    else
                        datafound |= HandleWrite(i.key(), i.value());
                }
                m_pLock.unlock();
            }
        }
    }
    RunEpilog();
}

bool MythSystemLegacyIOHandler::HandleRead(HANDLE h, QBuffer *buff)
{
    DWORD lenAvail;

    if ( !PeekNamedPipe( h, nullptr, 0, nullptr, &lenAvail, nullptr) )
        return false;

    if ( lenAvail > 65536 )
        lenAvail = 65536;

    DWORD lenRead;

    if ( !ReadFile( h, &m_readbuf, lenAvail, &lenRead, nullptr ) || lenRead == 0 )
    {
        m_pMap.remove(h);
        return false;
    }

    buff->buffer().append(m_readbuf, lenRead);

    // Get the corresponding MythSystemLegacy instance, and the stdout/stderr
    // type
    fdLock.lock();
    FDType_t *fdType = fdMap.value(h);
    fdLock.unlock();

    // Emit the data ready signal (1 = stdout, 2 = stderr)
    MythSystemLegacyWindows *ms = fdType->ms;
    emit ms->readDataReady(fdType->type);

    return true;
}

bool MythSystemLegacyIOHandler::HandleWrite(HANDLE h, QBuffer *buff)
{
    if( buff->atEnd() )
    {
        m_pMap.remove(h);
        return false;
    }

    int pos = buff->pos();
    DWORD len = buff->size() - pos;
    DWORD rlen;
    len = (len > 32768 ? 32768 : len);

    if( !WriteFile(h, buff->read(len).constData(), len, &rlen, nullptr) )
    {
        m_pMap.remove(h);
        return false;
    }

    if( rlen != len )
        buff->seek(pos+rlen);

    return true;
}

void MythSystemLegacyIOHandler::insert(HANDLE h, QBuffer *buff)
{
    m_pLock.lock();
    m_pMap.insert(h, buff);
    m_pLock.unlock();
    wake();
}

void MythSystemLegacyIOHandler::Wait(HANDLE h)
{
    QMutexLocker locker(&m_pLock);
    while (m_pMap.contains(h))
    {
        locker.unlock();
        usleep(10ms);
        locker.relock();
    }
}

void MythSystemLegacyIOHandler::remove(HANDLE h)
{
    m_pLock.lock();
    if (m_read)
    {
        PMap_t::iterator i;
        i = m_pMap.find(h);
        HandleRead(i.key(), i.value());
    }
    m_pMap.remove(h);
    m_pLock.unlock();
}

void MythSystemLegacyIOHandler::wake()
{
    QMutexLocker locker(&m_pWaitLock);
    m_pWait.wakeAll();
}


MythSystemLegacyManager::~MythSystemLegacyManager()
{
    if (m_children)
        free( m_children );
    wait();
}

void MythSystemLegacyManager::run(void)
{
    RunProlog();

    LOG(VB_GENERAL, LOG_INFO, "Starting process manager");

    // run_system is set to false during shutdown, and we need this thread to
    // exit during shutdown.
    while( run_system )
    {
        // check for any running processes
        m_mapLock.lock();

        if( m_childCount == 0 )
        {
            m_mapLock.unlock();
            usleep( 100ms );
            continue;
        }

        DWORD result = WaitForMultipleObjects( m_childCount, m_children,
                                               FALSE, 100 );

        if ( result == WAIT_TIMEOUT || result == WAIT_FAILED )
        {
            m_mapLock.unlock();
            continue;
        }

        int index = result - WAIT_OBJECT_0;
        if ( index < 0 || index > m_childCount - 1 )
        {
            m_mapLock.unlock();
            continue;
        }
        HANDLE child = m_children[index];

        // pop exited process off managed list, add to cleanup list
        MythSystemLegacyWindows  *ms = m_pMap.take(child);
        ChildListRebuild();
        m_mapLock.unlock();

        // Occasionally, the caller has deleted the structure from under
        // our feet.  If so, just log and move on.
        if (!ms || !ms->m_parent)
        {
            LOG(VB_SYSTEM, LOG_ERR,
                QString("Structure for child handle %1 already deleted!")
                .arg((long long)child));
            if (ms)
            {
                listLock.lock();
                msList.append(ms);
                listLock.unlock();
            }
            continue;
        }

        listLock.lock();
        msList.append(ms);

        DWORD               status;
        GetExitCodeProcess( child, &status );

        ms->SetStatus(status);
        LOG(VB_SYSTEM, LOG_INFO,
                QString("Managed child (Handle: %1) has exited! "
                        "command=%2, status=%3, result=%4")
                .arg((long long)child) .arg(ms->GetLogCmd()) .arg(status)
                .arg(ms->GetStatus()));

        // loop through running processes for any that require action
        MSMap_t::iterator   i;
        auto now = SystemClock::now();

        m_mapLock.lock();
        m_jumpLock.lock();
        for (i = m_pMap.begin(); i != m_pMap.end(); ++i)
        {
            child = i.key();
            ms    = i.value();

            // handle processes beyond marked timeout
            if( ms->m_timeout.time_since_epoch() > 0s && ms->m_timeout < now )
            {
                // issuing KILL signal after TERM failed in a timely manner
                if( ms->GetStatus() == GENERIC_EXIT_TIMEOUT )
                {
                    LOG(VB_SYSTEM, LOG_INFO,
                        QString("Managed child (Handle: %1) timed out, "
                                "issuing KILL signal").arg((long long)child));
                    // Prevent constant attempts to kill an obstinate child
                    ms->m_timeout = SystemTime(0s);
                    ms->Signal(SIGKILL);
                }

                // issuing TERM signal
                else
                {
                    LOG(VB_SYSTEM, LOG_INFO,
                        QString("Managed child (Handle: %1) timed out"
                                ", issuing TERM signal").arg((long long)child));
                    ms->SetStatus( GENERIC_EXIT_TIMEOUT );
                    ms->m_timeout = now + 1s;
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

    RunEpilog();
}

// NOTE: This is only to be run while m_mapLock is locked!!!
void MythSystemLegacyManager::ChildListRebuild()
{
    int                 oldCount;

    oldCount = m_childCount;
    m_childCount = m_pMap.size();

    MSMap_t::iterator   i;
    int                 j;
    HANDLE              child;

    if ( oldCount != m_childCount )
    {
        HANDLE *new_children;
        new_children = (HANDLE *)realloc(m_children,
                                         m_childCount * sizeof(HANDLE));
        if (!new_children && m_childCount)
        {
            LOG(VB_SYSTEM, LOG_CRIT, "No memory to allocate new children");
            free(m_children);
            m_children = nullptr;
            return;
        }

        m_children = new_children;
    }

    for (i = m_pMap.begin(), j = 0; i != m_pMap.end(); ++i)
    {
        child = i.key();
        m_children[j++] = child;
    }
}

void MythSystemLegacyManager::append(MythSystemLegacyWindows *ms)
{
    m_mapLock.lock();
    ms->IncrRef();
    m_pMap.insert(ms->m_child, ms);
    ChildListRebuild();
    m_mapLock.unlock();

    if (ms->m_stdpipe[0])
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
void MythSystemLegacySignalManager::run(void)
{
    RunProlog();

    LOG(VB_GENERAL, LOG_INFO, "Starting process signal handler");
    while( run_system )
    {
        usleep(50ms);
        while( run_system )
        {
            // handle cleanup and signalling for closed processes
            listLock.lock();
            if( msList.isEmpty() )
            {
                listLock.unlock();
                break;
            }
            MythSystemLegacyWindows *ms = msList.takeFirst();
            listLock.unlock();

            if (!ms)
                continue;

            if (ms->m_parent)
            {
                ms->m_parent->HandlePostRun();
            }

            if (ms->m_stdpipe[0])
                writeThread->remove(ms->m_stdpipe[0]);
            CLOSE(ms->m_stdpipe[0]);

            if (ms->m_stdpipe[1])
                readThread->remove(ms->m_stdpipe[1]);
            CLOSE(ms->m_stdpipe[1]);

            if (ms->m_stdpipe[2])
                readThread->remove(ms->m_stdpipe[2]);
            CLOSE(ms->m_stdpipe[2]);

            if (ms->m_parent)
            {
                if( ms->GetStatus() == GENERIC_EXIT_OK )
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

MythSystemLegacyWindows::MythSystemLegacyWindows(MythSystemLegacy *parent) :
    MythSystemLegacyPrivate("MythSystemLegacyWindows")
{
    m_parent = parent;

    m_stdpipe[0] = nullptr;
    m_stdpipe[1] = nullptr;
    m_stdpipe[2] = nullptr;

    connect(this, &MythSystemLegacyPrivate::started, m_parent, &MythSystemLegacy::started);
    connect(this, &MythSystemLegacyPrivate::finished, m_parent, &MythSystemLegacy::finished);
    connect(this, &MythSystemLegacyPrivate::error, m_parent,&MythSystemLegacy::error);
    connect(this, &MythSystemLegacyPrivate::readDataReady,
            m_parent, &MythSystemLegacy::readDataReady);

    // Start the threads if they haven't been started yet.
    if (manager == nullptr)
    {
        manager = new MythSystemLegacyManager;
        manager->start();
    }

    if (smanager == nullptr)
    {
        smanager = new MythSystemLegacySignalManager;
        smanager->start();
    }

    if (readThread == nullptr)
    {
        readThread = new MythSystemLegacyIOHandler(true);
        readThread->start();
    }

    if (writeThread == nullptr)
    {
        writeThread = new MythSystemLegacyIOHandler(false);
        writeThread->start();
    }
}

bool MythSystemLegacyWindows::ParseShell(const QString&, QString &, QStringList&)
{
    return false;
}

void MythSystemLegacyWindows::Term(bool force)
{
    if( (GetStatus() != GENERIC_EXIT_RUNNING) || (!m_child) )
        return;

    Signal(SIGTERM);
    if( force )
    {
        // send KILL if it does not exit within one second
        if( m_parent->Wait(1s) == GENERIC_EXIT_RUNNING )
            Signal(SIGKILL);
    }
}

void MythSystemLegacyWindows::Signal( int sig )
{
    if( (GetStatus() != GENERIC_EXIT_RUNNING) || (!m_child) )
        return;
    LOG(VB_SYSTEM, LOG_INFO, QString("Child Handle %1 killed with %2")
                    .arg((long long)m_child).arg(sig));
    TerminateProcess( m_child, sig * 256 );
}


#define MAX_BUFLEN 1024
void MythSystemLegacyWindows::Fork(std::chrono::seconds timeout)
{
    BOOL bInherit = FALSE;

    QString LOC_ERR = QString("myth_system('%1'): Error: ").arg(GetLogCmd());

    LOG(VB_SYSTEM, LOG_DEBUG, QString("Launching: %1").arg(GetLogCmd()));

    HANDLE p_stdin[2] = { nullptr, nullptr };
    HANDLE p_stdout[2] = { nullptr, nullptr };
    HANDLE p_stderr[2] = { nullptr, nullptr };

    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO si;

    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);

    // Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = true;
    saAttr.lpSecurityDescriptor = nullptr;

    /* set up pipes */
    if( GetSetting("UseStdin") )
    {
        bInherit = TRUE;

        if (!CreatePipe(&p_stdin[0], &p_stdin[1], &saAttr, 0))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC_ERR + "stdin pipe() failed");
            SetStatus( GENERIC_EXIT_NOT_OK );
        }
        else
        {
            // Ensure the write handle to the pipe for STDIN is not inherited.
            if (!SetHandleInformation(p_stdin[1], HANDLE_FLAG_INHERIT, 0))
            {
                LOG(VB_SYSTEM, LOG_ERR, LOC_ERR + "stdin inheritance error");
                SetStatus( GENERIC_EXIT_NOT_OK );
            }
            else
            {
                si.hStdInput = p_stdin[0];
                si.dwFlags |= STARTF_USESTDHANDLES;
            }
        }
    }

    if( GetSetting("UseStdout") )
    {
        bInherit = TRUE;

        if (!CreatePipe(&p_stdout[0], &p_stdout[1], &saAttr, 0))
        {
            LOG(VB_SYSTEM, LOG_ERR, LOC_ERR + "stdout pipe() failed");
            SetStatus( GENERIC_EXIT_NOT_OK );
        }
        else
        {
            // Ensure the read handle to the pipe for STDOUT is not inherited.
            if (!SetHandleInformation(p_stdout[0], HANDLE_FLAG_INHERIT, 0))
            {
                LOG(VB_SYSTEM, LOG_ERR, LOC_ERR + "stdout inheritance error");
                SetStatus( GENERIC_EXIT_NOT_OK );
            }
            else
            {
                si.hStdOutput = p_stdout[1];
                si.dwFlags |= STARTF_USESTDHANDLES;
            }
        }
    }

    if( GetSetting("UseStderr") )
    {
        bInherit = TRUE;

        if (!CreatePipe(&p_stderr[0], &p_stderr[1], &saAttr, 0))
        {
            LOG(VB_SYSTEM, LOG_ERR, LOC_ERR + "stderr pipe() failed");
            SetStatus( GENERIC_EXIT_NOT_OK );
        }
        else
        {
            // Ensure the read handle to the pipe for STDERR is not inherited.
            if (!SetHandleInformation(p_stderr[0], HANDLE_FLAG_INHERIT, 0))
            {
                LOG(VB_SYSTEM, LOG_ERR, LOC_ERR + "stderr inheritance error");
                SetStatus( GENERIC_EXIT_NOT_OK );
            }
            else
            {
                si.hStdError = p_stderr[1];
                si.dwFlags |= STARTF_USESTDHANDLES;
            }
        }
    }

    // set up command args
    QString cmd = GetCommand() + " " + GetArgs().join(" ");

    if (GetSetting("UseShell"))
        cmd.prepend("cmd.exe /c ");

    SetCommand( cmd );

    QString sCmd = GetCommand();

    QString dir = GetDirectory();

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    m_timeout = (timeout != 0s)
        ? SystemClock::now() + timeout
        : SystemClock::time_point();

    LPCWSTR pDir = nullptr;
    if (dir.length() > 0)
        pDir = (LPCWSTR)dir.utf16();

    char sCmdChar[256];
    sprintf(sCmdChar, "%ls", (LPWSTR)sCmd.utf16() );

    char pDirChar[256];
    sprintf(pDirChar, "%ls", pDir);

    bool success = CreateProcess( nullptr,
                          sCmdChar,       // command line
                          nullptr,       // process security attributes
                          nullptr,       // primary thread security attributes
                          bInherit,      // handles are inherited
                          0,             // creation flags
                          nullptr,       // use parent's environment
                          pDirChar,          // use parent's current directory
                          &si,            // STARTUPINFO pointer
                          &pi);           // receives PROCESS_INFORMATION

    if (!success)
    {
        DWORD dwErr = GetLastError();
        LOG(VB_SYSTEM, LOG_ERR,
                QString( "%1 CreateProcess() failed (%2)")
                    .arg( LOC_ERR )
                    .arg( dwErr ));
        SetStatus( GENERIC_EXIT_NOT_OK );
    }
    else
    {
        /* parent */
        m_child = pi.hProcess;
        SetStatus( GENERIC_EXIT_RUNNING );

        LOG(VB_SYSTEM, LOG_INFO,
                QString("Managed child (Handle: %1) has started! "
                        "%2%3 command=%4, timeout=%5")
                    .arg((long long)m_child)
                    .arg(GetSetting("UseShell") ? "*" : "")
                    .arg(GetSetting("RunInBackground") ? "&" : "")
                    .arg(GetLogCmd()).arg(timeout.count()));

        /* close unused pipe ends */
        CLOSE(p_stdin[0]);
        CLOSE(p_stdout[1]);
        CLOSE(p_stderr[1]);

        // store the rest
        m_stdpipe[0] = p_stdin[1];
        m_stdpipe[1] = p_stdout[0];
        m_stdpipe[2] = p_stderr[0];

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

void MythSystemLegacyWindows::Manage(void)
{
    if( manager == nullptr )
    {
        manager = new MythSystemLegacyManager;
        manager->start();
    }
    manager->append(this);
}

void MythSystemLegacyWindows::JumpAbort(void)
{
    if( manager == nullptr )
    {
        manager = new MythSystemLegacyManager;
        manager->start();
    }
    manager->jumpAbort();
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
