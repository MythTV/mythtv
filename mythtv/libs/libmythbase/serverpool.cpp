#include <QNetworkAddressEntry>
#include <QReadWriteLock>
#include <QWriteLocker>
#include <QReadLocker>

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "serverpool.h"

#define PRETTYIP(x)      x->protocol() == QAbstractSocket::IPv6Protocol ? \
                                    "[" + x->toString().toLower() + "]" : \
                                          x->toString().toLower()
#define PRETTYIP_(x)      x.protocol() == QAbstractSocket::IPv6Protocol ? \
                                    "[" + x.toString().toLower() + "]" : \
                                          x.toString().toLower()

#define LOC QString("ServerPool: ")

static QList<QNetworkAddressEntry> naList_4;
static QList<QNetworkAddressEntry> naList_6;
static QReadWriteLock naLock;

class PrivUdpSocket : public QUdpSocket
{
public:
    PrivUdpSocket(QObject *parent, QNetworkAddressEntry host) :
        QUdpSocket(parent), m_host(host) { };
    ~PrivUdpSocket() { };
    QNetworkAddressEntry host()
    {
        return m_host;
    };
    bool contains(QHostAddress addr)
    {
        return contains(m_host, addr);
    };
    static bool contains(QNetworkAddressEntry host, QHostAddress addr)
    {
#if !defined(QT_NO_IPV6)
        if (addr.protocol() == QAbstractSocket::IPv6Protocol &&
            addr.isInSubnet(QHostAddress::parseSubnet("fe80::/10")) &&
            host.ip().scopeId() != addr.scopeId())
        {
            return false;
        }
#endif
        return addr.isInSubnet(host.ip(), host.prefixLength());
    }
private:
    QNetworkAddressEntry m_host;
};

PrivTcpServer::PrivTcpServer(QObject *parent) : QTcpServer(parent)
{
}

void PrivTcpServer::incomingConnection(int socket)
{
    emit newConnection(socket);
}

ServerPool::ServerPool(QObject *parent) : QObject(parent),
    m_listening(false), m_maxPendingConn(30), m_port(0),
    m_proxy(QNetworkProxy::DefaultProxy), m_lastUdpSocket(NULL)
{
}

ServerPool::~ServerPool()
{
    close();
}

void ServerPool::SelectDefaultListen(bool force)
{
    if (!force)
    {
        QReadLocker rlock(&naLock);
        if (!naList_4.isEmpty() || !naList_6.isEmpty())
            // lists are already populated, do nothing
            return;
    }

    QWriteLocker wlock(&naLock);
    naList_4.clear();
    naList_6.clear();

    // populate stored IPv4 and IPv6 addresses
    // if address is not defined, QHostAddress will be invalid and Null
    QHostAddress config_v4(gCoreContext->GetSetting("BackendServerIP"));
    bool v4IsSet = config_v4.isNull() ? true : false;
#if !defined(QT_NO_IPV6)
    QHostAddress config_v6(gCoreContext->GetSetting("BackendServerIP6"));
    bool v6IsSet = config_v6.isNull() ? true : false;
#endif

    // loop through all available interfaces
    QList<QNetworkInterface> IFs = QNetworkInterface::allInterfaces();
    QList<QNetworkInterface>::const_iterator qni;
    for (qni = IFs.begin(); qni != IFs.end(); ++qni)
    {
        if ((qni->flags() & QNetworkInterface::IsRunning) == 0)
            continue;

        QList<QNetworkAddressEntry> IPs = qni->addressEntries();
        QList<QNetworkAddressEntry>::const_iterator qnai;
        for (qnai = IPs.begin(); qnai != IPs.end(); ++qnai)
        {
            QHostAddress ip = qnai->ip();
            if (naList_4.contains(*qnai))
                // already defined, skip
                continue;
            else if (!config_v4.isNull() && (ip == config_v4))
            {
                // IPv4 address is defined, add it
                LOG(VB_GENERAL, LOG_DEBUG,
                        QString("Adding BackendServerIP to address list."));
                naList_4.append(*qnai);
                v4IsSet = true;
            }
            else if (ip == QHostAddress::LocalHost)
            {
                // always listen on LocalHost
                LOG(VB_GENERAL, LOG_DEBUG,
                        QString("Adding IPv4 loopback to address list."));
                naList_4.append(*qnai);
                if (!v4IsSet && (config_v4 == ip))
                    v4IsSet = true;
            }
            else if (config_v4.isNull() &&
                     (ip.protocol() == QAbstractSocket::IPv4Protocol))
            {
                // IPv4 address is not defined, populate one
                // restrict autoconfiguration to RFC1918 private networks
                if (ip.isInSubnet(QHostAddress::parseSubnet("10.0.0.0/8")) ||
                    ip.isInSubnet(QHostAddress::parseSubnet("172.16.0.0/12")) ||
                    ip.isInSubnet(QHostAddress::parseSubnet("192.168.0.0/16")))
                {
                    LOG(VB_GENERAL, LOG_DEBUG,
                            QString("Adding '%1' to address list.")
                                .arg(ip.toString()));
                    naList_4.append(*qnai);
                }
                else
                    LOG(VB_GENERAL, LOG_DEBUG, QString("Skipping non-private "
                            "address during IPv4 autoselection: %1")
                                .arg(ip.toString()));
            }
#if !defined(QT_NO_IPV6)
            else if (naList_6.contains(*qnai))
                // already defined, skip
                continue;
            else if (!config_v6.isNull() && (ip == config_v6))
            {
                // IPv6 address is defined, add it
                LOG(VB_GENERAL, LOG_DEBUG,
                        QString("Adding BackendServerIP6 to address list."));
                naList_6.append(*qnai);
                v6IsSet = true;
            }
            else if (ip == QHostAddress::LocalHostIPv6)
            {
                // always listen on LocalHost
                LOG(VB_GENERAL, LOG_DEBUG,
                        QString("Adding IPv6 loopback to address list."));
                naList_6.append(*qnai);
                if (!v6IsSet && (config_v6 == ip))
                    v6IsSet = true;
            }
            else if (config_v6.isNull() &&
                     (ip.protocol() == QAbstractSocket::IPv6Protocol))
            {
                bool linklocal = false;
                if (ip.isInSubnet(QHostAddress::parseSubnet("fe80::/10")))
                {
                    // Link-local address, find its scope ID (interface name)
                    QNetworkAddressEntry ae = *qnai;
                    QHostAddress ha = ip;
                    ha.setScopeId(qni->name());
                    ae.setIp(ha);
                    naList_6.append(ae);
                    linklocal = true;
                }
                else
                {
                    naList_6.append(*qnai);
                }
                LOG(VB_GENERAL, LOG_DEBUG,
                        QString("Adding%1 '%2' to address list.")
                    .arg(linklocal ? " link-local" : "")
                            .arg(PRETTYIP_(ip)));
            }
#endif
            else
                LOG(VB_GENERAL, LOG_DEBUG, QString("Skipping address: %1")
                    .arg(PRETTYIP_(ip)));
        }
    }

    if (!v4IsSet && (config_v4 != QHostAddress::LocalHost)
                 && !naList_4.isEmpty())
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC + QString("Host is configured to listen "
                "on %1, but address is not used on any local network "
                "interfaces.").arg(config_v4.toString()));
    }

#if !defined(QT_NO_IPV6)
    if (!v6IsSet && (config_v6 != QHostAddress::LocalHostIPv6)
                 && !naList_6.isEmpty())
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC + QString("Host is configured to listen "
                "on %1, but address is not used on any local network "
                "interfaces.").arg(PRETTYIP_(config_v6)));
    }
#endif

    // NOTE: there is no warning for the case where both defined addresses
    //       are localhost, and neither are found. however this would also
    //       mean there is no configured network at all, and should be
    //       sufficiently rare a case as to not worry about it.
}

void ServerPool::RefreshDefaultListen(void)
{
    SelectDefaultListen(true);
    // TODO:
    // send signal for any running servers to cycle their addresses
    // will allow address configuration to be modified without a server
    // reset for use with the migration to the mythtv-setup replacement
}

QList<QHostAddress> ServerPool::DefaultListen(void)
{
    QList<QHostAddress> alist;
    alist << DefaultListenIPv4()
          << DefaultListenIPv6();
    return alist;
}

QList<QHostAddress> ServerPool::DefaultListenIPv4(void)
{
    SelectDefaultListen();
    QReadLocker rlock(&naLock);

    QList<QHostAddress> alist;
    QList<QNetworkAddressEntry>::const_iterator it;
    for (it = naList_4.begin(); it != naList_4.end(); ++it)
        if (!alist.contains(it->ip()))
            alist << it->ip();

    return alist;
}

QList<QHostAddress> ServerPool::DefaultListenIPv6(void)
{
    SelectDefaultListen();
    QReadLocker rlock(&naLock);

    QList<QHostAddress> alist;
    QList<QNetworkAddressEntry>::const_iterator it;
    for (it = naList_6.begin(); it != naList_6.end(); ++it)
        if (!alist.contains(it->ip()))
            alist << it->ip();

    return alist;
}

QList<QHostAddress> ServerPool::DefaultBroadcast(void)
{
    QList<QHostAddress> blist;
    blist << DefaultBroadcastIPv4();
    return blist;
}

QList<QHostAddress> ServerPool::DefaultBroadcastIPv4(void)
{
    SelectDefaultListen();
    QReadLocker rlock(&naLock);

    QList<QHostAddress> blist;
    QList<QNetworkAddressEntry>::const_iterator it;
    for (it = naList_4.begin(); it != naList_4.end(); ++it)
        if (!blist.contains(it->broadcast()) && (it->prefixLength() != 32) &&
                (it->ip() != QHostAddress::LocalHost))
            blist << it->broadcast();

    return blist;
}

//QList<QHostAddress> ServerPool::DefaultBroadcastIPv6(void)
//{
//
//}


void ServerPool::close(void)
{
    QTcpServer *server;
    while (!m_tcpServers.isEmpty())
    {
        server = m_tcpServers.takeLast();
        server->disconnect();
        server->close();
        server->deleteLater();
    }

    PrivUdpSocket *socket;
    while (!m_udpSockets.isEmpty())
    {
        socket = m_udpSockets.takeLast();
        socket->disconnect();
        socket->close();
        socket->deleteLater();
    }
    m_lastUdpSocket = NULL;
    m_listening = false;
}

bool ServerPool::listen(QList<QHostAddress> addrs, quint16 port,
                        bool requireall)
{
    m_port = port;
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
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed listening on TCP %1:%2 - Error %3: %4")
                        .arg(PRETTYIP(it))
                        .arg(port)
                        .arg(server->serverError())
                        .arg(server->errorString()));
            server->disconnect();
            server->deleteLater();

            if (requireall)
            {
                close();
                return false;
            }
        }
    }

    if (m_tcpServers.size() == 0)
        return false;

    m_listening = true;
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
    return listen(DefaultListen(), port, requireall);
}

bool ServerPool::bind(QList<QHostAddress> addrs, quint16 port,
                      bool requireall)
{
    m_port = port;
    QList<QHostAddress>::const_iterator it;

    for (it = addrs.begin(); it != addrs.end(); ++it)
    {
        QNetworkAddressEntry host;

#if !defined(QT_NO_IPV6)
        if (it->protocol() == QAbstractSocket::IPv6Protocol)
        {
            QList<QNetworkAddressEntry>::iterator iae;
            for (iae = naList_6.begin(); iae != naList_6.end(); ++iae)
            {
                if (PrivUdpSocket::contains(*iae, *it))
                {
                    host = *iae;
                    break;
                }
            }
        }
        else
#endif
        {
            QList<QNetworkAddressEntry>::iterator iae;
            for (iae = naList_4.begin(); iae != naList_4.end(); ++iae)
            {
                if (PrivUdpSocket::contains(*iae, *it))
                {
                    host = *iae;
                    break;
                }
            }
        }

        PrivUdpSocket *socket = new PrivUdpSocket(this, host);

        if (socket->bind(*it, port))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Binding to UDP %1:%2")
                    .arg(PRETTYIP(it)).arg(port));
            m_udpSockets.append(socket);
            connect(socket, SIGNAL(readyRead()),
                    this,   SLOT(newUdpDatagram()));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed binding to UDP %1:%2 - Error %3: %4")
                        .arg(PRETTYIP(it))
                        .arg(port)
                        .arg(socket->error())
                        .arg(socket->errorString()));
            socket->disconnect();
            socket->deleteLater();

            if (requireall)
            {
                close();
                return false;
            }
        }
    }

    if (m_udpSockets.size() == 0)
        return false;

    m_listening = true;
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
    return bind(DefaultListen(), port, requireall);
}

qint64 ServerPool::writeDatagram(const char * data, qint64 size,
                                 const QHostAddress &addr, quint16 port)
{
    if (!m_listening || m_udpSockets.size() == 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to write datagram to disconnected "
                            "ServerPool instance.");
        return -1;
    }

    // check if can re-use the last one, so there's no need for a linear search
    if (!m_lastUdpSocket || !m_lastUdpSocket->contains(addr))
    {
        // find the right socket to use
        QList<PrivUdpSocket*>::iterator it;
        for (it = m_udpSockets.begin(); it != m_udpSockets.end(); ++it)
        {
            PrivUdpSocket *val = *it;
            if (val->contains(addr))
            {
                m_lastUdpSocket = val;
                break;
            }
        }
    }
    if (!m_lastUdpSocket)
        return -1;

    qint64 ret = m_lastUdpSocket->writeDatagram(data, size, addr, port);
    if (ret != size)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Error = %1 : %2")
                .arg(ret).arg(m_lastUdpSocket->error()));
    }
    return ret;
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

/**
 * tryListeningPort
 *
 * Description:
 * Tells the server to listen for incoming connections on port port.
 * The server will attempt to listen on all local interfaces.
 *
 * Usage:
 * baseport: port to listen on.
 * range:    range of ports to try (default 1)
 *
 * Returns port used on success; otherwise returns -1.
 */
int ServerPool::tryListeningPort(int baseport, int range)
{
    // try a few ports in case the first is in use
    int port = baseport;
    while (port < baseport + range)
    {
        if (listen(port))
        {
            break;
        }
        port++;
    }

    if (port >= baseport + range)
    {
        return -1;
    }
    return port;
}

/**
 * tryBindingPort
 *
 * Description:
 * Binds this socket for incoming connections on port port.
 * The socket will attempt to bind on all local interfaces.
 *
 * Usage:
 * baseport: port to bind to.
 * range:    range of ports to try (default 1)
 *
 * Returns port used on success; otherwise returns -1.
 */
int ServerPool::tryBindingPort(int baseport, int range)
{
    // try a few ports in case the first is in use
    int port = baseport;
    while (port < baseport + range)
    {
        if (bind(port))
        {
            break;
        }
        port++;
    }

    if (port >= baseport + range)
    {
        return -1;
    }
    return port;
}

/**
 * tryListeningPort
 *
 * Description:
 * Tells the server to listen for incoming connections on port port.
 * The server will attempt to listen on all IPv6 and IPv4 interfaces.
 * If IPv6 isn't available, the server will listen on all IPv4 network interfaces.
 *
 * Usage:
 * server:   QTcpServer object to use
 * baseport: port to listen on. If port is 0, a port is chosen automatically.
 * range:    range of ports to try (default 1)
 * isipv6:   is set to true if IPv6 was successful (default NULL)
 *
 * Returns port used on success; otherwise returns -1.
 */
int ServerPool::tryListeningPort(QTcpServer *server, int baseport,
                                 int range, bool *isipv6)
{
    bool ipv6 = true;
    // try a few ports in case the first is in use
    int port = baseport;
    while (port < baseport + range)
    {
        if (ipv6)
        {
            if (server->listen(QHostAddress::AnyIPv6, port))
            {
                break;
            }
            else
            {
                // did we fail because IPv6 isn't available?
                QAbstractSocket::SocketError err = server->serverError();
                if (err == QAbstractSocket::UnsupportedSocketOperationError)
                {
                    ipv6 = false;
                }
            }
        }
        if (!ipv6)
        {
            if (server->listen(QHostAddress::Any, port))
            {
                break;
            }
        }
        port++;
    }

    if (isipv6)
    {
        *isipv6 = ipv6;
    }

    if (port >= baseport + range)
    {
        return -1;
    }
    if (port == 0)
    {
        port = server->serverPort();
    }
    return port;
}

/**
 * tryBindingPort
 *
 * Description:
 * Binds this socket for incoming connections on port port.
 * The socket will attempt to bind on all IPv6 and IPv4 interfaces.
 * If IPv6 isn't available, the socket will be bound to all IPv4 network interfaces.
 *
 * Usage:
 * socket:   QUdpSocket object to use
 * baseport: port to bind to.
 * range:    range of ports to try (default 1)
 * isipv6:   is set to true if IPv6 was successful (default NULL)
 *
 * Returns port used on success; otherwise returns -1.
 */
int ServerPool::tryBindingPort(QUdpSocket *socket, int baseport,
                               int range, bool *isipv6)
{
    bool ipv6 = true;
    // try a few ports in case the first is in use
    int port = baseport;
    while (port < baseport + range)
    {
        if (ipv6)
        {
            if (socket->bind(QHostAddress::AnyIPv6, port))
            {
                break;
            }
            else
            {
                // did we fail because IPv6 isn't available?
                QAbstractSocket::SocketError err = socket->error();
                if (err == QAbstractSocket::UnsupportedSocketOperationError)
                {
                    ipv6 = false;
                }
            }
        }
        if (!ipv6)
        {
            if (socket->bind(QHostAddress::Any, port))
            {
                break;
            }
        }
        port++;
    }

    if (isipv6)
    {
        *isipv6 = ipv6;
    }

    if (port >= baseport + range)
    {
        return -1;
    }
    return port;
}
