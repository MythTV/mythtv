// Qt
#include <QMutex>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QEvent>
#include <QCoreApplication>
#include <QNetworkProxy>
#include <QRunnable>

// MythTV
#include "libmythbase/mthread.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/referencecounter.h"
#include "libmythbase/serverpool.h"

#include "mythsocketmanager.h"
#include "sockethandler.h"
#include "socketrequesthandler.h"

#define LOC      QString("MythSocketManager: ")

static constexpr std::chrono::milliseconds PRT_TIMEOUT { 10ms };

class ProcessRequestRunnable : public QRunnable
{
  public:
    ProcessRequestRunnable(MythSocketManager &parent, MythSocket *sock) :
        m_parent(parent), m_sock(sock)
    {
        m_sock->IncrRef();
    }

    void run(void) override // QRunnable
    {
        m_parent.ProcessRequest(m_sock);
        m_sock->DecrRef();
        m_sock = nullptr;
    }

    MythSocketManager &m_parent;
    MythSocket        *m_sock;
};

MythSocketManager::MythSocketManager() :
    m_threadPool("MythSocketManager")
{
}

MythSocketManager::~MythSocketManager()
{
    m_threadPool.Stop();

    QWriteLocker wlock(&m_handlerLock);

    QMap<QString, SocketRequestHandler*>::iterator i;
    for (i = m_handlerMap.begin(); i != m_handlerMap.end(); ++i)
        delete *i;

    m_handlerMap.clear();

    QMutexLocker locker(&m_socketListLock);
    for (auto iter = m_socketList.begin();
         iter != m_socketList.end();
         iter = m_socketList.erase(iter))
    {
        (*iter)->DecrRef();
    }
}

bool MythSocketManager::Listen(int port)
{
    if (m_server != nullptr)
    {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }

    m_server = new MythServer(this);
    m_server->setProxy(QNetworkProxy::NoProxy);
    if (!m_server->listen(port))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to bind port %1.").arg(port));
        return false;
    }

    connect(m_server, &MythServer::newConnection,
            this,     &MythSocketManager::newConnection);
    return true;
}

void MythSocketManager::newConnection(qintptr sd)
{
    QMutexLocker locker(&m_socketListLock);
    auto *ms = new MythSocket(sd, this);
    if (ms->IsConnected())
        m_socketList.insert(ms);
    else
        delete ms;
}

void MythSocketManager::RegisterHandler(SocketRequestHandler *handler)
{
    QWriteLocker wlock(&m_handlerLock);

    QString name = handler->GetHandlerName();
    if (m_handlerMap.contains(name))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + name +
            " has already been registered.");
        delete handler;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "Registering socket command handler " + name);
        handler->SetParent(this);
        m_handlerMap.insert(name, handler);
    }
}

void MythSocketManager::AddSocketHandler(SocketHandler *sock)
{
    QWriteLocker wlock(&m_socketLock);
    if (m_socketMap.contains(sock->GetSocket()))
        return;

    sock->IncrRef();
    m_socketMap.insert(sock->GetSocket(), sock);
}

SocketHandler *MythSocketManager::GetConnectionBySocket(MythSocket *sock)
{
    QReadLocker rlock(&m_socketLock);
    if (!m_socketMap.contains(sock))
        return nullptr;

    SocketHandler *handler = m_socketMap[sock];
    handler->IncrRef();
    return handler;
}

void MythSocketManager::readyRead(MythSocket *sock)
{
    m_threadPool.startReserved(
        new ProcessRequestRunnable(*this, sock),
        "ServiceRequest", PRT_TIMEOUT);
}

void MythSocketManager::connectionClosed(MythSocket *sock)
{
    // TODO We should delete the MythSocket's at some point
    // prior to MythSocketManager shutdown...

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
            handler->DecrRef();
        }
    }
}

void MythSocketManager::ProcessRequest(MythSocket *sock)
{
    // used as context manager since MythSocket cannot be used directly 
    // with QMutexLocker

    if (sock->IsDataAvailable())
    {
        ProcessRequestWork(sock);
    }
}

void MythSocketManager::ProcessRequestWork(MythSocket *sock)
{
    QStringList listline;
    if (!sock->ReadStringList(listline))
        return;

    QString line = listline[0].simplified();
    QStringList tokens = line.split(' ', Qt::SkipEmptyParts);
    QString command = tokens[0];

    bool handled = false;

    // DONE can be called at any time
    if (command == "DONE")
    {
        HandleDone(sock);
        return;
    }

    if (!sock->IsValidated())
    {
        // all sockets must be validated against the local protocol version
        // before any subsequent commands can be run
        if (command == "MYTH_PROTO_VERSION")
        {
            HandleVersion(sock, tokens);
        }
        else
        {
            LOG(VB_SOCKET, LOG_ERR, LOC +
                "Use of socket attempted before protocol validation.");
            listline.clear();
            listline << "ERROR" << "socket has not been validated";
            sock->WriteStringList(listline);
        }
        return;
    }

    if (!sock->IsAnnounced())
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
                LOG(VB_SOCKET, LOG_DEBUG, LOC +
                    QString("Attempting to handle announce with: %1")
                        .arg((*i)->GetHandlerName()));
                handled = (*i)->HandleAnnounce(sock, tokens, listline);
                ++i;
            }

            if (handled)
            {
                --i;
                LOG(VB_SOCKET, LOG_DEBUG, LOC +
                    QString("Socket announce handled by: %1")
                        .arg((*i)->GetHandlerName()));
                for (i = m_handlerMap.constBegin(); 
                         i != m_handlerMap.constEnd(); ++i)
                    (*i)->connectionAnnounced(sock, tokens, listline);
            }
            else
            {
                LOG(VB_SOCKET, LOG_ERR, LOC + "Socket announce unhandled.");
                listline.clear();
                listline << "ERROR" << "unhandled announce";
                sock->WriteStringList(listline);
            }
            
            return;
        }
        LOG(VB_SOCKET, LOG_ERR, LOC +
            "Use of socket attempted before announcement.");
        listline.clear();
        listline << "ERROR" << "socket has not been announced";
        sock->WriteStringList(listline);
        return;
    }

    if (command == "ANN")
    {
        LOG(VB_SOCKET, LOG_ERR, LOC + "ANN sent out of sequence.");
        listline.clear();
        listline << "ERROR" << "socket has already been announced";
        sock->WriteStringList(listline);
        return;
    }

    {
        // socket is validated and announced, handle everything else
        QReadLocker rlock(&m_handlerLock);
        QReadLocker slock(&m_socketLock);

        if (!m_socketMap.contains(sock))
        {
            LOG(VB_SOCKET, LOG_ERR, LOC + "No handler found for socket.");
            listline.clear();
            listline << "ERROR" << "socket handler cannot be found";
            sock->WriteStringList(listline);
            return;
        }

        SocketHandler *handler = GetConnectionBySocket(sock);
        ReferenceLocker hlock(handler);

        QMap<QString, SocketRequestHandler*>::const_iterator i
                            = m_handlerMap.constBegin();
        while (!handled && (i != m_handlerMap.constEnd()))
        {
            handled = (*i)->HandleQuery(handler, tokens, listline);
            ++i;
        }

        if (handled)
            LOG(VB_SOCKET, LOG_DEBUG, LOC + QString("Query handled by: %1")
                        .arg((*--i)->GetHandlerName()));
    }

    if (!handled)
    {
        if (command == "BACKEND_MESSAGE")
        {
            // never respond to these... ever, even if they are not otherwise
            // handled by something in m_handlerMap
            return;
        }

        listline.clear();
        listline << "ERROR" << "unknown command";
        sock->WriteStringList(listline);
    }
}

void MythSocketManager::HandleVersion(MythSocket *socket,
                                      const QStringList &slist)
{
    QStringList retlist;
    const QString& version = slist[1];
    if (version != MYTH_PROTO_VERSION)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Client speaks protocol version " + version +
            " but we speak " + MYTH_PROTO_VERSION + '!');
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        socket->WriteStringList(retlist);
        HandleDone(socket);
        return;
    }

    if (slist.size() < 3)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Client did not pass protocol "
                                       "token. Refusing connection!");
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        socket->WriteStringList(retlist);
        HandleDone(socket);
        return;
    }

    const QString& token = slist[2];
    if (token != QString::fromUtf8(MYTH_PROTO_TOKEN))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Client sent incorrect protocol token \"%1\" for "
                    "protocol version. Refusing connection!").arg(token));
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        socket->WriteStringList(retlist);
        HandleDone(socket);
        return;
    }

    LOG(VB_SOCKET, LOG_DEBUG, LOC + "Client validated");
    retlist << "ACCEPT" << MYTH_PROTO_VERSION;
    socket->WriteStringList(retlist);
    socket->m_isValidated = true;
}

void MythSocketManager::HandleDone(MythSocket *sock)
{
    sock->DisconnectFromHost();
}

