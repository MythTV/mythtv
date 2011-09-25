// ANSI C
#include <cstdlib>

// C++
#include <algorithm> // for min/max
//using namespace std;

#include "compat.h"

// POSIX
#ifndef USING_MINGW
#include <sys/select.h> // for select
#endif
#include <sys/types.h>  // for fnctl
#include <fcntl.h>      // for fnctl
#include <errno.h>      // for checking errno

#ifndef O_NONBLOCK
#define O_NONBLOCK 0 /* not actually supported in MINGW */
#endif

// Qt
#include <QTime>

// MythTV
#include "mythsocketthread.h"
#include "mythbaseutil.h"
#include "mythlogging.h"
#include "mythsocket.h"

#define SLOC(a) QString("MythSocketThread(sock 0x%1:%2): ")\
                    .arg((uint64_t)a, 0, 16).arg(a->socket())
#define LOC     QString("MythSocketThread: ")

const uint MythSocketThread::kShortWait = 100;

MythSocketThread::MythSocketThread()
    : MThread("Socket"), m_readyread_run(false)
{
    for (int i = 0; i < 2; i++)
    {
        m_readyread_pipe[i] = -1;
        m_readyread_pipe_flags[i] = 0;
    }
}

void ShutdownRRT(void)
{
    QMutexLocker locker(&MythSocket::s_readyread_thread_lock);
    if (MythSocket::s_readyread_thread)
    {
        MythSocket::s_readyread_thread->ShutdownReadyReadThread();
        MythSocket::s_readyread_thread->wait();
    }
}

void MythSocketThread::ShutdownReadyReadThread(void)
{
    {
        QMutexLocker locker(&m_readyread_lock);
        m_readyread_run = false;
    }

    WakeReadyReadThread();

    wait(); // waits for thread to exit

    CloseReadyReadPipe();
}

void MythSocketThread::CloseReadyReadPipe(void) const
{
    for (uint i = 0; i < 2; i++)
    {
        if (m_readyread_pipe[i] >= 0)
        {
            ::close(m_readyread_pipe[i]);
            m_readyread_pipe[i] = -1;
            m_readyread_pipe_flags[i] = 0;
        }
    }
}

void MythSocketThread::StartReadyReadThread(void)
{
    QMutexLocker locker(&m_readyread_lock);
    if (!m_readyread_run)
    {
        atexit(ShutdownRRT);
        setup_pipe(m_readyread_pipe, m_readyread_pipe_flags);
        m_readyread_run = true;
        start();
        m_readyread_started_wait.wait(&m_readyread_lock);
    }
}

void MythSocketThread::AddToReadyRead(MythSocket *sock)
{
    if (sock->socket() == -1)
    {
        LOG(VB_SOCKET, LOG_ERR, SLOC(sock) +
                "attempted to insert invalid socket to ReadyRead");
        return;
    }
    StartReadyReadThread();

    sock->UpRef();

    {
        QMutexLocker locker(&m_readyread_lock);
        m_readyread_addlist.push_back(sock);
    }

    WakeReadyReadThread();
}

void MythSocketThread::RemoveFromReadyRead(MythSocket *sock)
{
    {
        QMutexLocker locker(&m_readyread_lock);
        m_readyread_dellist.push_back(sock);
    }
    WakeReadyReadThread();
}

void MythSocketThread::WakeReadyReadThread(void) const
{
    if (!isRunning())
        return;

    QMutexLocker locker(&m_readyread_lock);
    m_readyread_wait.wakeAll();

    if (m_readyread_pipe[1] < 0)
        return;

    char buf[1] = { '0' };
    ssize_t wret = 0;
    while (wret <= 0)
    {
        wret = ::write(m_readyread_pipe[1], &buf, 1);
        if ((wret < 0) && (EAGAIN != errno) && (EINTR != errno))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to write to readyread pipe, closing pipe.");

            // Closing the pipe will cause the run loop's select to exit.
            // Then the next time through the loop we should fallback to
            // using the code for platforms that don't support pipes..
            CloseReadyReadPipe();
            break;
        }
    }
}

void MythSocketThread::ReadyToBeRead(MythSocket *sock)
{
    LOG(VB_SOCKET, LOG_DEBUG, SLOC(sock) + "socket is readable");
    int bytesAvail = sock->bytesAvailable();
    
    if (bytesAvail == 0 && sock->closedByRemote())
    {
        LOG(VB_SOCKET, LOG_INFO, SLOC(sock) + "socket closed");
        sock->close();
    }
    else if (bytesAvail > 0 && sock->m_cb && sock->m_useReadyReadCallback)
    {
        sock->m_notifyread = true;
        LOG(VB_SOCKET, LOG_DEBUG, SLOC(sock) + "calling m_cb->readyRead()");
        sock->m_cb->readyRead(sock);
    }
}

void MythSocketThread::ProcessAddRemoveQueues(void)
{
    while (!m_readyread_dellist.empty())
    {
        MythSocket *sock = m_readyread_dellist.front();
        m_readyread_dellist.pop_front();

        if (m_readyread_list.removeAll(sock))
            m_readyread_downref_list.push_back(sock);
    }

    while (!m_readyread_addlist.empty())
    {
        MythSocket *sock = m_readyread_addlist.front();
        m_readyread_addlist.pop_front();
        m_readyread_list.push_back(sock);
    }
}

void MythSocketThread::run(void)
{
    RunProlog();
    LOG(VB_SOCKET, LOG_DEBUG, LOC + "readyread thread start");

    QMutexLocker locker(&m_readyread_lock);
    m_readyread_started_wait.wakeAll();
    while (m_readyread_run)
    {
        LOG(VB_SOCKET, LOG_DEBUG, LOC + "ProcessAddRemoveQueues");

        ProcessAddRemoveQueues();

        LOG(VB_SOCKET, LOG_DEBUG, LOC + "Construct FD_SET");

        // construct FD_SET for all connected and unlocked sockets...
        int maxfd = -1;
        fd_set rfds;
        FD_ZERO(&rfds);

        QList<MythSocket*>::const_iterator it = m_readyread_list.begin();
        for (; it != m_readyread_list.end(); ++it)
        {
            if (!(*it)->TryLock(false))
                continue;

            if ((*it)->state() == MythSocket::Connected &&
                !(*it)->m_notifyread)
            {
                FD_SET((*it)->socket(), &rfds);
                maxfd = std::max((*it)->socket(), maxfd);
            }
            (*it)->Unlock(false);
        }

        // There are no unlocked sockets, wait for event before we continue..
        if (maxfd < 0)
        {
            LOG(VB_SOCKET, LOG_DEBUG, LOC + "Empty FD_SET, sleeping");
            if (m_readyread_wait.wait(&m_readyread_lock))
                LOG(VB_SOCKET, LOG_DEBUG, LOC + "Empty FD_SET, woken up");
            else
                LOG(VB_SOCKET, LOG_DEBUG, LOC + "Empty FD_SET, timed out");
            continue;
        }

        int rval = 0;

        if (m_readyread_pipe[0] >= 0)
        {
            // Clear out any pending pipe reads, we have already taken care of
            // this event above under the m_readyread_lock.
            char dummy[128];
            if (m_readyread_pipe_flags[0] & O_NONBLOCK)
            {
                rval = ::read(m_readyread_pipe[0], dummy, 128);
                FD_SET(m_readyread_pipe[0], &rfds);
                maxfd = std::max(m_readyread_pipe[0], maxfd);
            }

            // also exit select on exceptions on same descriptors
            fd_set efds;
            memcpy(&efds, &rfds, sizeof(fd_set));

            // The select waits forever for data, so if we need to process
            // anything else we need to write to m_readyread_pipe[1]..
            // We unlock the ready read lock, because we don't need it
            // and this will allow WakeReadyReadThread() to run..
            m_readyread_lock.unlock();
            LOG(VB_SOCKET, LOG_DEBUG, LOC + "Waiting on select..");
            rval = select(maxfd + 1, &rfds, NULL, &efds, NULL);
            LOG(VB_SOCKET, LOG_DEBUG, LOC + "Got data on select");
            m_readyread_lock.lock();

            if (rval > 0 && FD_ISSET(m_readyread_pipe[0], &rfds))
            {
                int ret = ::read(m_readyread_pipe[0], dummy, 128);
                if (ret < 0)
                {
                    LOG(VB_SOCKET, LOG_ERR, LOC +
                        "Strange.. failed to read event pipe");
                }
            }
        }
        else
        {
            LOG(VB_SOCKET, LOG_DEBUG, LOC + "Waiting on select.. (no pipe)");

            fd_set savefds;
            memcpy(&savefds, &rfds, sizeof(fd_set));

            // Unfortunately, select on a pipe is not supported on all
            // platforms. So we fallback to a loop that instead times out
            // of select and checks for wakeAll event.
            while (!rval)
            {
                // also exit select on exceptions on same descriptors
                fd_set efds;
                memcpy(&efds, &savefds, sizeof(fd_set));

                struct timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = kShortWait * 1000;
                rval = select(maxfd + 1, &rfds, NULL, &efds, &timeout);
                if (!rval)
                {
                    m_readyread_wait.wait(&m_readyread_lock, kShortWait);
                    memcpy(&rfds, &savefds, sizeof(fd_set));
                }
            }

            if (rval > 0)
                LOG(VB_SOCKET, LOG_DEBUG, LOC + "Got data on select (no pipe)");
        }

        if (rval <= 0)
        {
            if (rval == 0)
            {
                // Note: This should never occur when using pipes. When there
                // is no error there should be data in at least one fd..
                LOG(VB_SOCKET, LOG_DEBUG, LOC + "select timeout");
            }
            else
                LOG(VB_SOCKET, LOG_ERR, LOC + "select returned error" + ENO);

            m_readyread_wait.wait(&m_readyread_lock, kShortWait);
            continue;
        }
        
        // ReadyToBeRead allows calls back into the socket so we need
        // to release the lock for a little while.
        // since only this loop updates m_readyread_list this is safe.
        m_readyread_lock.unlock();

        // Actually read some data! This is a form of co-operative
        // multitasking so the ready read handlers should be quick..

        uint downref_tm = 0;
        if (!m_readyread_downref_list.empty())
        {
            LOG(VB_SOCKET, LOG_DEBUG, LOC + "Deleting stale sockets");

            QTime tm = QTime::currentTime();
            for (it = m_readyread_downref_list.begin();
                 it != m_readyread_downref_list.end(); ++it)
            {
                (*it)->DownRef();
            }
            m_readyread_downref_list.clear();
            downref_tm = tm.elapsed();
        }

        LOG(VB_SOCKET, LOG_DEBUG, LOC + "Processing ready reads");

        QMap<uint,uint> timers;
        QTime tm = QTime::currentTime();
        it = m_readyread_list.begin();

        for (; it != m_readyread_list.end() && m_readyread_run; ++it)
        {
            if (!(*it)->TryLock(false))
                continue;
            
            int socket = (*it)->socket();

            if (socket >= 0 &&
                (*it)->state() == MythSocket::Connected &&
                FD_ISSET(socket, &rfds))
            {
                QTime rrtm = QTime::currentTime();
                ReadyToBeRead(*it);
                timers[socket] = rrtm.elapsed();
            }
            (*it)->Unlock(false);
        }

        if (VERBOSE_LEVEL_CHECK(VB_SOCKET, LOG_DEBUG))
        {
            QString rep = QString("Total read time: %1ms, on sockets")
                .arg(tm.elapsed());
            QMap<uint,uint>::const_iterator it = timers.begin();
            for (; it != timers.end(); ++it)
                rep += QString(" {%1,%2ms}").arg(it.key()).arg(*it);
            if (downref_tm)
                rep += QString(" {downref, %1ms}").arg(downref_tm);

            LOG(VB_SOCKET, LOG_DEBUG, LOC + rep);
        }

        m_readyread_lock.lock();
        LOG(VB_SOCKET, LOG_DEBUG, LOC + "Reacquired ready read lock");
    }

    LOG(VB_SOCKET, LOG_DEBUG, LOC + "readyread thread exit");
    RunEpilog();
}
