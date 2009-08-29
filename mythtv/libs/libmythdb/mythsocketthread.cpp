// ANSI C
#include <cstdlib>
#include <cassert>

// POSIX
#include <sys/select.h> // for select
#include <sys/types.h>  // for fnctl
#include <unistd.h>     // for fnctl & other
#include <fcntl.h>      // for fnctl
#include <errno.h>      // for checking errno

// Microsoft
#ifdef USING_MINGW
#include <winsock2.h>
#endif

// MythTV
#include "mythsocketthread.h"
#include "mythsocket.h"
#include "mythverbose.h"

#define SLOC(a) QString("MythSocketThread(sock 0x%1:%2): ")\
    .arg((quint64)a, 0, 16).arg(a->socket())

MythSocketThread::MythSocketThread()
    : QThread(), m_readyread_run(false)
{
#if !defined(USING_MINGW)
    memset(m_readyread_pipe, 0, sizeof(m_readyread_pipe));
#endif
}

void ShutdownRRT(void)
{
    MythSocket::s_readyread_thread->ShutdownReadyReadThread();
}

void MythSocketThread::ShutdownReadyReadThread(void)
{
    m_readyread_run = false;
    WakeReadyReadThread();
    wait();

#ifdef USING_MINGW
    if (readyreadevent) {
        ::CloseHandle(readyreadevent);
        readyreadevent = NULL;
    }
#else
    ::close(m_readyread_pipe[0]);
    ::close(m_readyread_pipe[1]);
#endif
}

void MythSocketThread::StartReadyReadThread(void)
{
    if (m_readyread_run == false)
    {
        QMutexLocker locker(&m_readyread_lock);
        if (m_readyread_run == false)
        {
#ifdef USING_MINGW
            readyreadevent = ::CreateEvent(NULL, false, false, NULL);
            assert(readyreadevent);
#else
            int ret = pipe(m_readyread_pipe);
            (void) ret;
            assert(ret >= 0);
#endif

            m_readyread_run = true;
            start();

            atexit(ShutdownRRT);
        }
    }
}

void MythSocketThread::AddToReadyRead(MythSocket *sock)
{
    if (sock->socket() == -1)
    {
        VERBOSE(VB_SOCKET, SLOC(sock) +
                "attempted to insert invalid socket to ReadyRead");
        return;
    }
    StartReadyReadThread();

    sock->UpRef();
    m_readyread_lock.lock();
    m_readyread_addlist.append(sock);
    m_readyread_lock.unlock();

    WakeReadyReadThread();
}

void MythSocketThread::RemoveFromReadyRead(MythSocket *sock)
{
    m_readyread_lock.lock();
    m_readyread_dellist.append(sock);
    m_readyread_lock.unlock();

    WakeReadyReadThread();
}

void MythSocketThread::WakeReadyReadThread(void)
{
    if (!isRunning())
        return;

#ifdef USING_MINGW
    if (readyreadevent) ::SetEvent(readyreadevent);
#else
    if (m_readyread_pipe[1] >= 0)
    {
        char buf[1] = { '0' };
        ssize_t wret = 0;
        while (wret <= 0)
        {
            wret = ::write(m_readyread_pipe[1], &buf, 1);
            if ((wret < 0) && (EAGAIN != errno) && (EINTR != errno))
            {
                VERBOSE(VB_IMPORTANT, "MythSocketThread, Error: "
                        "Irrecoverable WakeReadyReadThread event");
                break;
            }
        }
        // dtk note: add this aft dbg for correctness..
        //::flush(m_readyread_pipe[1]);
    }
#endif
}

void MythSocketThread::iffound(MythSocket *sock)
{
    VERBOSE(VB_SOCKET, SLOC(sock) + "socket is readable");
    if (sock->bytesAvailable() == 0)
    {
        VERBOSE(VB_SOCKET, SLOC(sock) + "socket closed");
        sock->close();

        if (sock->m_cb)
        {
            VERBOSE(VB_SOCKET, SLOC(sock) + "calling m_cb->connectionClosed()");
            sock->m_cb->connectionClosed(sock);
        }
    }
    else if (sock->m_cb)
    {
        sock->m_notifyread = true;
        VERBOSE(VB_SOCKET, SLOC(sock) + "calling m_cb->readyRead()");
        sock->m_cb->readyRead(sock);
    }
}

void MythSocketThread::run(void)
{
    VERBOSE(VB_SOCKET, "MythSocketThread: readyread thread start");

    fd_set rfds;
    MythSocket *sock;
    int maxfd;
    bool found;

    while (m_readyread_run)
    {
        m_readyread_lock.lock();
        while (m_readyread_dellist.size() > 0)
        {
            sock = m_readyread_dellist.takeFirst();
            bool del = m_readyread_list.removeAll(sock);

            if (del)
            {
                m_readyread_lock.unlock();
                sock->DownRef();
                m_readyread_lock.lock();
            }
        }

        while (m_readyread_addlist.count() > 0)
        {
            sock = m_readyread_addlist.takeFirst();
            //sock->UpRef();  Did upref in AddToReadyRead()
            m_readyread_list.append(sock);
        }
        m_readyread_lock.unlock();

#ifdef USING_MINGW

        int n = m_readyread_list.count() + 1;
        HANDLE *hEvents = new HANDLE[n];
        memset(hEvents, 0, sizeof(HANDLE) * n);
        unsigned *idx = new unsigned[n];
        n = 0;

        for (unsigned i = 0; i < (uint) m_readyread_list.count(); i++)
        {
            sock = m_readyread_list.at(i);
            if (sock->state() == MythSocket::Connected
                && !sock->m_notifyread && !isLocked(sock->m_lock))
            {
                HANDLE hEvent = ::CreateEvent(NULL, false, false, NULL);
                if (!hEvent)
                {
                    VERBOSE(VB_IMPORTANT, "CreateEvent failed");
                }
                else
                {
                    if (SOCKET_ERROR != ::WSAEventSelect(
                            sock->socket(), hEvent,
                            FD_READ | FD_CLOSE))
                    {
                        hEvents[n] = hEvent;
                        idx[n++] = i;
                    }
                    else
                    {
                        VERBOSE(VB_IMPORTANT, QString(
                                    "CreateEvent, "
                                    "WSAEventSelect(%1, %2) failed")
                                .arg(sock->socket()));
                        ::CloseHandle(hEvent);
                    }
                }
            }
        }

        hEvents[n++] = readyreadevent;
        int rval = ::WaitForMultipleObjects(n, hEvents, false, INFINITE);

        for (int i = 0; i < (n - 1); i++)
            ::CloseHandle(hEvents[i]);

        delete[] hEvents;

        if (rval == WAIT_FAILED)
        {
            VERBOSE(VB_IMPORTANT, "WaitForMultipleObjects returned error");
            delete[] idx;
        }
        else if (rval >= WAIT_OBJECT_0 && rval < (WAIT_OBJECT_0 + n))
        {
            rval -= WAIT_OBJECT_0;

            if (rval < (n - 1))
            {
                rval = idx[rval];
                sock = m_readyread_list.at(rval);
                found = (sock->state() == MythSocket::Connected)
                            && !isLocked(sock->m_lock);
            }
            else
            {
                found = false;
            }

            delete[] idx;

            if (found)
                iffound(sock);

            ::ResetEvent(readyreadevent);
        }
        else if (rval >= WAIT_ABANDONED_0 && rval < (WAIT_ABANDONED_0 + n))
        {
            VERBOSE(VB_SOCKET, "abandoned");
        }
        else
        {
            VERBOSE(VB_SOCKET|VB_EXTRA, "select timeout");
        }

#else /* if !USING_MINGW */

        // add check for bad fd?
        FD_ZERO(&rfds);
        maxfd = m_readyread_pipe[0];

        FD_SET(m_readyread_pipe[0], &rfds);

        QList<MythSocket*>::const_iterator it = m_readyread_list.begin();
        while (it != m_readyread_list.end())
        {
            sock = *it;
            if (sock->state() == MythSocket::Connected &&
                sock->m_notifyread == false &&
                !isLocked(sock->m_lock))
            {
                FD_SET(sock->socket(), &rfds);
                maxfd = std::max(sock->socket(), maxfd);
            }
            ++it;
        }

        int rval = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (rval == -1)
        {
            VERBOSE(VB_SOCKET, "MythSocketThread: select returned error");
        }
        else if (rval)
        {
            found = false;
            QList<MythSocket*>::const_iterator it = m_readyread_list.begin();
            while (it != m_readyread_list.end())
            {
                sock = *it;
                if (sock->state() == MythSocket::Connected &&
                    FD_ISSET(sock->socket(), &rfds) &&
                    !isLocked(sock->m_lock))
                {
                    found = true;
                    break;
                }
                ++it;
            }

            if (found)
                iffound(sock);

            if (FD_ISSET(m_readyread_pipe[0], &rfds))
            {
                char buf[128];
                ssize_t rr;

                do rr = ::read(m_readyread_pipe[0], buf, 128);
                while ((rr < 0) && (EINTR == errno));
            }
        }
        else
        {
            VERBOSE(VB_SOCKET|VB_EXTRA, "MythSocketThread: select timeout");
        }

#endif /* !USING_MINGW */

    }

    VERBOSE(VB_SOCKET, "MythSocketThread: readyread thread exit");
}

bool MythSocketThread::isLocked(QMutex &mutex)
{
    bool isLocked = true;
    if (mutex.tryLock())
    {
        mutex.unlock();
        isLocked = false;
    }
    return isLocked;
}
