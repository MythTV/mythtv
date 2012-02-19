
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "serverpool.h"

#define PRETTYIP(x)      x->protocol() == QAbstractSocket::IPv6Protocol ? \
                                    "[" + x->toString().toLower() + "]" : \
                                          x->toString().toLower()

PrivTcpServer::PrivTcpServer(QObject *parent) : QTcpServer(parent)
{
}

void PrivTcpServer::incomingConnection(int socket)
{
    emit newConnection(socket);
}

ServerPool::ServerPool(QObject *parent) : QObject(parent),
    m_listening(false), m_maxPendingConn(30), m_port(0),
    m_proxy(QNetworkProxy::DefaultProxy)
{
}

ServerPool::~ServerPool()
{
    close();
}

void ServerPool::close(void)
{
    QTcpServer *server;
    while (!m_tcpServers.isEmpty())
    {
        server = m_tcpServers.takeLast();
        server->disconnect();
        server->close();
        delete server;
    }

    QUdpSocket *socket;
    while (!m_udpSockets.isEmpty())
    {
        socket = m_udpSockets.takeLast();
        socket->disconnect();
        socket->close();
        delete socket;
    }
}

bool ServerPool::listen(QList<QHostAddress> addrs, quint16 port,
                        bool requireall)
{
    m_port = port;
    m_listening = true;
    QList<QHostAddress>::const_iterator it;

    for (it = addrs.begin(); it != addrs.end(); ++it)
    {
        PrivTcpServer *server = new PrivTcpServer(this);
        server->setProxy(m_proxy);
        server->setMaxPendingConnections(m_maxPendingConn);

        connect(server, SIGNAL(newConnection(int)),
                this,   SLOT(newTcpConnection(int)));

        if (server->listen(*it, m_port))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Listening on TCP %1:%2")
                    .arg(PRETTYIP(it)).arg(port));
            m_tcpServers.append(server);
            if (m_port == 0)
                m_port = server->serverPort();
        }
        else if (requireall)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed listening on TCP %1:%2")
                    .arg(PRETTYIP(it)).arg(port));
            close();
            server->disconnect();
            delete server;
            return false;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Failed listening on TCP %1:%2")
                    .arg(PRETTYIP(it)).arg(port));
            server->disconnect();
            delete server;
        }
    }

    if (m_tcpServers.size() == 0)
        return false;

    return true;
}

bool ServerPool::listen(QStringList addrstr, quint16 port, bool requireall)
{
    QList<QHostAddress> addrs;
    QStringList::const_iterator it;
    for (it = addrstr.begin(); it != addrstr.end(); ++it)
        addrs << QHostAddress(*it);
    return listen(addrs, port, requireall);
}

bool ServerPool::listen(quint16 port, bool requireall)
{
    return listen(gCoreContext->MythHostAddress(), port, requireall);
}

bool ServerPool::bind(QList<QHostAddress> addrs, quint16 port,
                      bool requireall)
{
    m_port = port;
    m_listening = true;
    QList<QHostAddress>::const_iterator it;

    for (it = addrs.begin(); it != addrs.end(); ++it)
    {
        QUdpSocket *socket = new QUdpSocket(this);
        connect(socket, SIGNAL(readyRead()),
                this,   SLOT(newUdpDatagram()));

        if (socket->bind(*it, port))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Binding to UDP %1:%2")
                    .arg(PRETTYIP(it)).arg(port));
            m_udpSockets.append(socket);
        }
        else if (requireall)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed binding to UDP %1:%2")
                    .arg(PRETTYIP(it)).arg(port));
            close();
            socket->disconnect();
            delete socket;
            return false;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Failed binding to UDP %1:%2")
                    .arg(PRETTYIP(it)).arg(port));
            socket->disconnect();
            delete socket;
        }
    }

    if (m_udpSockets.size() == 0)
        return false;

    return true;
}

bool ServerPool::bind(QStringList addrstr, quint16 port, bool requireall)
{
    QList<QHostAddress> addrs;
    QStringList::const_iterator it;
    for (it = addrstr.begin(); it != addrstr.end(); ++it)
        addrs << QHostAddress(*it);
    return bind(addrs, port, requireall);
}

bool ServerPool::bind(quint16 port, bool requireall)
{
    return bind(gCoreContext->MythHostAddress(), port, requireall);
}

qint64 ServerPool::writeDatagram(const char * data, qint64 size,
                                 const QHostAddress &addr, quint16 port)
{
    if (!m_listening || m_udpSockets.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to write datagram to disconnected "
                            "ServerPool instance.");
        return -1;
    }

    QUdpSocket *socket = m_udpSockets.first();
    return socket->writeDatagram(data, size, addr, port);
}

qint64 ServerPool::writeDatagram(const QByteArray &datagram,
                                 const QHostAddress &addr, quint16 port)
{
    return writeDatagram(datagram.data(), datagram.size(), addr, port);
}

void ServerPool::newTcpConnection(int socket)
{
    QTcpSocket *qsock = new QTcpSocket(this);
    qsock->setSocketDescriptor(socket);
    emit newConnection(qsock);
}

void ServerPool::newUdpDatagram(void)
{
    QUdpSocket *socket = dynamic_cast<QUdpSocket*>(sender());

    while (socket->state() == QAbstractSocket::BoundState &&
           socket->hasPendingDatagrams())
    {
        QByteArray buffer;
        buffer.resize(socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        socket->readDatagram(buffer.data(), buffer.size(),
                             &sender, &senderPort);
        emit newDatagram(buffer, sender, senderPort);
    }
}
