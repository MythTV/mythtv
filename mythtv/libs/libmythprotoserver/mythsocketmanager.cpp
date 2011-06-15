using namespace std;

// Qt
#include <QMutex>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QThread>
#include <QEvent>
#include <QCoreApplication>

// MythTV
#include "mythsocketmanager.h"
#include "socketrequesthandler.h"
#include "sockethandler.h"
#include "referencecounter.h"
#include "mythcorecontext.h"
#include "mythconfig.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "mythlogging.h"

#define LOC      QString("MythSocketManager: ")
#define LOC_WARN QString("MythSocketManager, Warning: ")
#define LOC_ERR  QString("MythSocketManager, Error: ")

#define PRT_STARTUP_THREAD_COUNT 2
#define PRT_TIMEOUT 10

uint socket_id = 1;

class ProcessRequestThread : public QThread
{
  public:
    ProcessRequestThread(MythSocketManager *ms):
        m_parent(ms), m_socket(NULL), m_threadlives(false) {}

    void setup(MythSocket *sock)
    {
        QMutexLocker locker(&m_lock);
        m_socket = sock;
        m_socket->UpRef();
        m_waitCond.wakeAll();
    }

    void killit(void)
    {
        QMutexLocker locker(&m_lock);
        m_threadlives = false;
        m_waitCond.wakeAll();
    }

    virtual void run(void)
    {
	threadRegister("ProcessRequest");
        QMutexLocker locker(&m_lock);
        m_threadlives = true;
        m_waitCond.wakeAll(); // Signal to creating thread

        while (true)
        {
            m_waitCond.wait(locker.mutex());
            VERBOSE(VB_SOCKET|VB_EXTRA, "ProcessRequestThread running.");

            if (!m_threadlives)
                break;

            if (!m_socket)
            {
                VERBOSE(VB_SOCKET|VB_EXTRA, "ProcessRequestThread has no target.");
                continue;
            }

            m_parent->ProcessRequest(m_socket);
            m_socket->DownRef();
            m_socket = NULL;
            m_parent->MarkUnused(this);
        }
	threadDeregister();
    }

    QMutex m_lock;
    QWaitCondition m_waitCond;

  private:
    MythSocketManager  *m_parent;
    MythSocket         *m_socket;
    bool                m_threadlives;
};

MythSocketManager::MythSocketManager() :
    m_server(NULL)
{
    SetThreadCount(PRT_STARTUP_THREAD_COUNT);
}

MythSocketManager::~MythSocketManager()
{
    QWriteLocker wlock(&m_handlerLock);

    QMap<QString, SocketRequestHandler*>::iterator i;
    for (i = m_handlerMap.begin(); i != m_handlerMap.end(); ++i)
        delete *i;

    m_handlerMap.clear();
}

void MythSocketManager::SetThreadCount(uint count)
{
    if (m_threadPool.size() >= count)
        return;

    VERBOSE(VB_IMPORTANT, QString("%1Increasing thread count to %2")
                .arg(LOC).arg(count));
    while (count > m_threadPool.size())
    {
        ProcessRequestThread *prt = new ProcessRequestThread(this);
        prt->m_lock.lock();
        prt->start();
        prt->m_waitCond.wait(&prt->m_lock);
        prt->m_lock.unlock();
        m_threadPool.push_back(prt);
    }
}

void MythSocketManager::MarkUnused(ProcessRequestThread *prt)
{
    VERBOSE(VB_SOCKET|VB_EXTRA, "Releasing ProcessRequestThread.");

    QMutexLocker locker(&m_threadPoolLock);
    m_threadPool.push_back(prt);
    m_threadPoolCond.wakeAll();

    VERBOSE(VB_SOCKET|VB_EXTRA, QString("ProcessRequestThread pool size: %1")
                                        .arg(m_threadPool.size()));
}

bool MythSocketManager::Listen(int port)
{
    if (m_server != NULL)
    {
        m_server->close();
        delete m_server;
        m_server = NULL;
    }

    m_server = new MythServer();
    if (!m_server->listen(QHostAddress(gCoreContext->MythHostAddressAny()), port))
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to bind port %1.").arg(port));
        return false;
    }

    connect(m_server, SIGNAL(newConnect(MythSocket *)),
            SLOT(newConnection(MythSocket *)));
    return true;
}

void MythSocketManager::RegisterHandler(SocketRequestHandler *handler)
{
    QWriteLocker wlock(&m_handlerLock);

    QString name = handler->GetHandlerName();
    if (m_handlerMap.contains(name))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + name + 
                                    " has already been registered.");
        delete handler;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC + "Registering socket command handler " + 
                                    name);
        handler->SetParent(this);
        m_handlerMap.insert(name, handler);
    }
}

void MythSocketManager::AddSocketHandler(SocketHandler *sock)
{
    QWriteLocker wlock(&m_socketLock);
    if (m_socketMap.contains(sock->GetSocket()))
        return;

    sock->UpRef();
    m_socketMap.insert(sock->GetSocket(), sock);
}

SocketHandler *MythSocketManager::GetConnectionBySocket(MythSocket *sock)
{
    QReadLocker rlock(&m_socketLock);
    if (!m_socketMap.contains(sock))
        return NULL;

    SocketHandler *handler = m_socketMap[sock];
    handler->UpRef();
    return handler;
}

void MythSocketManager::readyRead(MythSocket *sock)
{
    if (sock->isExpectingReply())
    {
        VERBOSE(VB_SOCKET|VB_EXTRA, "Socket marked as expecting reply.");
        return;
    }

    ProcessRequestThread *prt = NULL;
    {
        QMutexLocker locker(&m_threadPoolLock);

        if (m_threadPool.empty())
        {
            VERBOSE(VB_GENERAL, "Waiting for a process request thread.. ");
            m_threadPoolCond.wait(&m_threadPoolLock, PRT_TIMEOUT);
        }

        if (!m_threadPool.empty())
        {
            prt = m_threadPool.front();
            m_threadPool.pop_front();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Adding a new process request thread");
            prt = new ProcessRequestThread(this);
            prt->m_lock.lock();
            prt->start();
            prt->m_waitCond.wait(&prt->m_lock);
            prt->m_lock.unlock();
        }
    }

    prt->setup(sock);
}

void MythSocketManager::connectionClosed(MythSocket *sock)
{
    {
        QReadLocker rlock(&m_handlerLock);

        QMap<QString, SocketRequestHandler*>::const_iterator i;
        for (i = m_handlerMap.constBegin(); i != m_handlerMap.constEnd(); ++i)
            (*i)->connectionClosed(sock); 
    }

    {
        QWriteLocker wlock(&m_socketLock);
        if (m_socketMap.contains(sock))
        {
            SocketHandler *handler = m_socketMap.take(sock);
            handler->DownRef();
        }
    }
}

void MythSocketManager::ProcessRequest(MythSocket *sock)
{
    // used as context manager since MythSocket cannot be used directly 
    // with QMutexLocker
    
    if (sock->isExpectingReply())
        return;

    sock->Lock();

    if (sock->bytesAvailable() > 0)
    {
        ProcessRequestWork(sock);
    }

    sock->Unlock();
}

void MythSocketManager::ProcessRequestWork(MythSocket *sock)
{
    QStringList listline;
    if (!sock->readStringList(listline))
        return;

    QString line = listline[0].simplified();
    QStringList tokens = line.split(' ', QString::SkipEmptyParts);
    QString command = tokens[0];

    bool handled = false;

    // DONE can be called at any time
    if (command == "DONE")
    {
        HandleDone(sock);
        return;
    }

    if (!sock->isValidated())
    {
        // all sockets must be validated against the local protocol version
        // before any subsequent commands can be run
        if (command == "MYTH_PROTO_VERSION")
        {
            HandleVersion(sock, tokens);
        }
        else
        {
            VERBOSE(VB_SOCKET, LOC_ERR +
                    "Use of socket attempted before protocol validation.");
            listline.clear();
            listline << "ERROR" << "socket has not been validated";
            sock->writeStringList(listline);
        }
        return;
    }

    if (!sock->isAnnounced())
    {
        // all sockets must be announced before any subsequent commands can
        // be run
        if (command == "ANN")
        {
            QReadLocker rlock(&m_handlerLock);

            QMap<QString, SocketRequestHandler*>::const_iterator i
                            = m_handlerMap.constBegin();
            while (!handled && (i != m_handlerMap.constEnd()))
            {
                VERBOSE(VB_SOCKET|VB_EXTRA, LOC +
                            QString("Attempting to handle annouce with: %1")
                                    .arg((*i)->GetHandlerName()));
                handled = (*i)->HandleAnnounce(sock, tokens, listline);
                i++;
            }

            if (handled)
            {
                i--;
                VERBOSE(VB_SOCKET, LOC +
                            QString("Socket announce handled by: %1")
                                    .arg((*i)->GetHandlerName()));
                for (i = m_handlerMap.constBegin(); 
                         i != m_handlerMap.constEnd(); ++i)
                    (*i)->connectionAnnounced(sock, tokens, listline);
            }
            else
            {
                VERBOSE(VB_SOCKET, LOC_ERR + "Socket announce unhandled.");
                listline.clear();
                listline << "ERROR" << "unhandled announce";
                sock->writeStringList(listline);
            }
            
            return;
        }
        else
        {
            VERBOSE(VB_SOCKET, LOC_ERR +
                            "Use of socket attempted before announcement.");
            listline.clear();
            listline << "ERROR" << "socket has not been announced";
            sock->writeStringList(listline);
        }
        return;
    }

    if (command == "ANN")
    {
        VERBOSE(VB_SOCKET, LOC_ERR + "ANN sent out of sequence.");
        listline.clear();
        listline << "ERROR" << "socket has already been announced";
        sock->writeStringList(listline);
        return;
    }

    {
        // socket is validated and announced, handle everything else
        QReadLocker rlock(&m_handlerLock);
        QReadLocker slock(&m_socketLock);

        if (!m_socketMap.contains(sock))
        {
            VERBOSE(VB_SOCKET, LOC_ERR + "No handler found for socket.");
            listline.clear();
            listline << "ERROR" << "socket handler cannot be found";
            sock->writeStringList(listline);
            return;
        }

        SocketHandler *handler = GetConnectionBySocket(sock);
        ReferenceLocker hlock(handler, false);

        QMap<QString, SocketRequestHandler*>::const_iterator i
                            = m_handlerMap.constBegin();
        while (!handled && (i != m_handlerMap.constEnd()))
        {
            handled = (*i)->HandleQuery(handler, tokens, listline);
            i++;
        }

        if (handled)
            VERBOSE(VB_SOCKET, QString("Query handled by: %1")
                        .arg((*--i)->GetHandlerName()));

    }

    if (!handled)
    {
        listline.clear();
        listline << "ERROR" << "unknown command";
        sock->writeStringList(listline);
    }
}

void MythSocketManager::HandleVersion(MythSocket *socket,
                                      const QStringList slist)
{
    QStringList retlist;
    QString version = slist[1];
    if (version != MYTH_PROTO_VERSION)
    {
        VERBOSE(VB_GENERAL,
                "MainServer::HandleVersion - Client speaks protocol version "
                + version + " but we speak " + MYTH_PROTO_VERSION + '!');
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        socket->writeStringList(retlist);
        HandleDone(socket);
        return;
    }

    if (slist.size() < 3)
    {
        VERBOSE(VB_GENERAL,
                "MainServer::HandleVersion - Client did not pass protocol "
                "token. Refusing connection!");
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        socket->writeStringList(retlist);
        HandleDone(socket);
        return;
    }

    QString token = slist[2];
    if (token != MYTH_PROTO_TOKEN)
    {
        VERBOSE(VB_GENERAL,
                "MainServer::HandleVersion - Client sent incorrect protocol"
                " token for protocol version. Refusing connection!");
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        socket->writeStringList(retlist);
        HandleDone(socket);
        return;
    }

    VERBOSE(VB_SOCKET, "MainServer::HandlerVersion - Client validated");
    retlist << "ACCEPT" << MYTH_PROTO_VERSION;
    socket->writeStringList(retlist);
    socket->setValidated();
}

void MythSocketManager::HandleDone(MythSocket *sock)
{
    sock->close();
}

void MythServer::incomingConnection(int socket)
{
    MythSocket *s = new MythSocket(socket);
    emit newConnect(s);
}
