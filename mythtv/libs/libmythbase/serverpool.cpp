#include <QNetworkAddressEntry>
#include <QReadWriteLock>
#include <QWriteLocker>
#include <QReadLocker>
#include <QTcpSocket>

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "serverpool.h"

static inline
QString prettyip(const QHostAddress& x)
{
    if (x.protocol() == QAbstractSocket::IPv6Protocol)
        return "[" + x.toString().toLower() + "]";
    return x.toString().toLower();
}


#define LOC QString("ServerPool: ")

// Lists of IP address this machine is listening to.
static QList<QNetworkAddressEntry> naList_4;
static QList<QNetworkAddressEntry> naList_6;
static QReadWriteLock naLock;

static QPair<QHostAddress, int> kLinkLocal  =
                            QHostAddress::parseSubnet("169.254.0.0/16");
static QPair<QHostAddress, int> kLinkLocal6 =
                            QHostAddress::parseSubnet("fe80::/10");

class PrivUdpSocket : public QUdpSocket
{
public:
    PrivUdpSocket(QObject *parent, const QNetworkAddressEntry& host) :
        QUdpSocket(parent), m_host(host) { };
    ~PrivUdpSocket() override = default;
    QNetworkAddressEntry host()
    {
        return m_host;
    };
    bool contains(const QHostAddress& addr)
    {
        return contains(m_host, addr);
    };
    static bool contains(const QNetworkAddressEntry& host, const QHostAddress& addr)
    {
        if (addr.protocol() == QAbstractSocket::IPv6Protocol &&
            addr.isInSubnet(kLinkLocal6) &&
            host.ip().scopeId() != addr.scopeId())
        {
            return false;
        }
        return addr.isInSubnet(host.ip(), host.prefixLength());
    }
private:
    QNetworkAddressEntry m_host;
};

PrivTcpServer::PrivTcpServer(QObject *parent, PoolServerType type)
              : QTcpServer(parent), m_serverType(type)
{
}

void PrivTcpServer::incomingConnection(qintptr socket)
{
    emit newConnection(socket);
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

    if (gCoreContext->GetBoolSetting("ListenOnAllIps",true))
    {
        QNetworkAddressEntry entry;
        entry.setIp(QHostAddress(QHostAddress::AnyIPv4));
        naList_4.append(entry);
        entry.setIp(QHostAddress(QHostAddress::AnyIPv6));
        naList_6.append(entry);
        return;
    }

    // populate stored IPv4 and IPv6 addresses
    QHostAddress config_v4(gCoreContext->resolveSettingAddress(
                                           "BackendServerIP",
                                           QString(),
                                           MythCoreContext::ResolveIPv4, true));
    bool v4IsSet = config_v4.isNull();
    QHostAddress config_v6(gCoreContext->resolveSettingAddress(
                                           "BackendServerIP6",
                                           QString(),
                                           MythCoreContext::ResolveIPv6, true));
    bool v6IsSet = config_v6.isNull();
    bool allowLinkLocal = gCoreContext->GetBoolSetting("AllowLinkLocal", true);

    // loop through all available interfaces
    QList<QNetworkInterface> IFs = QNetworkInterface::allInterfaces();
    for (const auto & qni : std::as_const(IFs))
    {
        if ((qni.flags() & QNetworkInterface::IsRunning) == 0)
            continue;

        QList<QNetworkAddressEntry> IPs = qni.addressEntries();
        // C+11 range-for should only be used on const Qt lists, and
        // this next loop modifies the list items.
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto qnai = IPs.begin(); qnai != IPs.end(); ++qnai)
        {
            QHostAddress ip = qnai->ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol)
            {
                if (naList_4.contains(*qnai))
                    // already defined, skip
                    continue;

                if (!config_v4.isNull() && (ip == config_v4))
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

                else if (ip.isInSubnet(kLinkLocal) && allowLinkLocal)
                {
                    // optionally listen on linklocal
                    // the next clause will enable it anyway if no IP address
                    // has been set
                    LOG(VB_GENERAL, LOG_DEBUG,
                            QString("Adding link-local '%1' to address list.")
                                .arg(prettyip(ip)));
                    naList_4.append(*qnai);
                }

                else if (config_v4.isNull())
                {
                    // IPv4 address is not defined, populate one
                    // restrict autoconfiguration to RFC1918 private networks
                    static QPair<QHostAddress, int>
                       s_privNet1 = QHostAddress::parseSubnet("10.0.0.0/8");
                    static QPair<QHostAddress, int>
                       s_privNet2 = QHostAddress::parseSubnet("172.16.0.0/12");
                    static QPair<QHostAddress, int>
                       s_privNet3 = QHostAddress::parseSubnet("192.168.0.0/16");

                    if (ip.isInSubnet(s_privNet1) || ip.isInSubnet(s_privNet2) ||
                        ip.isInSubnet(s_privNet3))
                    {
                        LOG(VB_GENERAL, LOG_DEBUG,
                                QString("Adding '%1' to address list.")
                                    .arg(prettyip(ip)));
                        naList_4.append(*qnai);
                    }
                    else if (ip.isInSubnet(kLinkLocal))
                    {
                        LOG(VB_GENERAL, LOG_DEBUG,
                            QString("Adding link-local '%1' to address list.")
                                    .arg(prettyip(ip)));
                        naList_4.append(*qnai);
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_DEBUG, QString("Skipping "
                           "non-private address during IPv4 autoselection: %1")
                                    .arg(prettyip(ip)));
                    }
                }

                else
                {
                    LOG(VB_GENERAL, LOG_DEBUG, QString("Skipping address: %1")
                                .arg(prettyip(ip)));
                }

            }
            else
            {
                if (ip.isInSubnet(kLinkLocal6))
                {
                    // set scope id for link local address
                    ip.setScopeId(qni.name());
                    qnai->setIp(ip);
                }

                if (naList_6.contains(*qnai))
                    // already defined, skip
                    continue;

                if ((!config_v6.isNull()) && (ip == config_v6))
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

                else if (ip.isInSubnet(kLinkLocal6) && allowLinkLocal)
                {
                    // optionally listen on linklocal
                    // the next clause will enable it anyway if no IP address
                    // has been set
                    LOG(VB_GENERAL, LOG_DEBUG,
                            QString("Adding link-local '%1' to address list.")
                                .arg(ip.toString()));
                    naList_6.append(*qnai);
                }

                else if (config_v6.isNull())
                {
                    if (ip.isInSubnet(kLinkLocal6))
                    {
                        LOG(VB_GENERAL, LOG_DEBUG,
                            QString("Adding link-local '%1' to address list.")
                                .arg(prettyip(ip)));
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_DEBUG,
                            QString("Adding '%1' to address list.")
                                .arg(prettyip(ip)));
                    }

                    naList_6.append(*qnai);
                }

                else
                {
                    LOG(VB_GENERAL, LOG_DEBUG, QString("Skipping address: %1")
                                .arg(prettyip(ip)));
                }
            }
        }
    }

    if (!v4IsSet && (config_v4 != QHostAddress::LocalHost)
                 && !naList_4.isEmpty())
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC + QString("Host is configured to listen "
                "on %1, but address is not used on any local network "
                "interfaces.").arg(config_v4.toString()));
    }

    if (!v6IsSet && (config_v6 != QHostAddress::LocalHostIPv6)
                 && !naList_6.isEmpty())
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC + QString("Host is configured to listen "
                "on %1, but address is not used on any local network "
                "interfaces.").arg(prettyip(config_v6)));
    }

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
    for (const auto & nae : std::as_const(naList_4))
        if (!alist.contains(nae.ip()))
            alist << nae.ip();

    return alist;
}

QList<QHostAddress> ServerPool::DefaultListenIPv6(void)
{
    SelectDefaultListen();
    QReadLocker rlock(&naLock);

    QList<QHostAddress> alist;
    for (const auto & nae : std::as_const(naList_6))
        if (!alist.contains(nae.ip()))
            alist << nae.ip();

    return alist;
}

QList<QHostAddress> ServerPool::DefaultBroadcast(void)
{
    QList<QHostAddress> blist;
    if (!gCoreContext->GetBoolSetting("ListenOnAllIps",true))
    {
        blist << DefaultBroadcastIPv4();
        blist << DefaultBroadcastIPv6();
    }
    return blist;
}

QList<QHostAddress> ServerPool::DefaultBroadcastIPv4(void)
{
    SelectDefaultListen();
    QReadLocker rlock(&naLock);

    QList<QHostAddress> blist;
    for (const auto & nae : std::as_const(naList_4))
    {
        if (!blist.contains(nae.broadcast()) && (nae.prefixLength() != 32) &&
                (nae.ip() != QHostAddress::LocalHost))
            blist << nae.broadcast();
    }

    return blist;
}

QList<QHostAddress> ServerPool::DefaultBroadcastIPv6(void)
{
    QList<QHostAddress> blist;
    blist << QHostAddress("FF02::1");
    return blist;
}


void ServerPool::close(void)
{
    while (!m_tcpServers.isEmpty())
    {
        PrivTcpServer *server = m_tcpServers.takeLast();
        server->disconnect();
        server->close();
        server->deleteLater();
    }

    while (!m_udpSockets.isEmpty())
    {
        PrivUdpSocket *socket = m_udpSockets.takeLast();
        socket->disconnect();
        socket->close();
        socket->deleteLater();
    }
    m_lastUdpSocket = nullptr;
    m_listening = false;
}

bool ServerPool::listen(QList<QHostAddress> addrs, quint16 port,
                        bool requireall, PoolServerType servertype)
{
    m_port = port;
    for (const auto & qha : std::as_const(addrs))
    {
        // If IPV4 support is disabled and this is an IPV4 address,
        // bypass this address
        if (qha.protocol() == QAbstractSocket::IPv4Protocol
          && ! gCoreContext->GetBoolSetting("IPv4Support",true))
            continue;
        // If IPV6 support is disabled and this is an IPV6 address,
        // bypass this address
        if (qha.protocol() == QAbstractSocket::IPv6Protocol
          && ! gCoreContext->GetBoolSetting("IPv6Support",true))
            continue;

        auto *server = new PrivTcpServer(this, servertype);
            connect(server, &PrivTcpServer::newConnection,
                this,   &ServerPool::newTcpConnection);

        server->setProxy(m_proxy);
        server->setMaxPendingConnections(m_maxPendingConn);

        if (server->listen(qha, m_port))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Listening on TCP%1 %2:%3")
                .arg(servertype == kSSLServer ? " (SSL)" : "",
                     prettyip(qha), QString::number(port)));
            if ((servertype == kTCPServer) || (servertype == kSSLServer))
                m_tcpServers.append(server);
            if (m_port == 0)
                m_port = server->serverPort();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed listening on TCP %1:%2 - Error %3: %4")
                        .arg(prettyip(qha))
                        .arg(port)
                        .arg(server->serverError())
                        .arg(server->errorString()));
            server->disconnect();
            server->deleteLater();

            if (server->serverError() == QAbstractSocket::HostNotFoundError ||
                server->serverError() == QAbstractSocket::SocketAddressNotAvailableError)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Address %1 no longer exists - ignoring")
                    .arg(prettyip(qha)));
                continue;
            }

            if (server->serverError() == QAbstractSocket::UnsupportedSocketOperationError
                 && qha.protocol() == QAbstractSocket::IPv4Protocol)
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("IPv4 support failed for this port."));
                 continue;
            }

            if (server->serverError() == QAbstractSocket::UnsupportedSocketOperationError
                 && qha.protocol() == QAbstractSocket::IPv6Protocol)
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("IPv6 support failed for this port."));
                 continue;
            }

            if (requireall)
            {
                close();
                return false;
            }
        }
    }

    if (m_tcpServers.empty())
        return false;

    m_listening = true;
    return true;
}

bool ServerPool::listen(QStringList addrstr, quint16 port, bool requireall,
                        PoolServerType servertype)
{
    QList<QHostAddress> addrs;
    for (const auto & str : std::as_const(addrstr))
        addrs << QHostAddress(str);
    return listen(addrs, port, requireall, servertype);
}

bool ServerPool::listen(quint16 port, bool requireall,
                        PoolServerType servertype)
{
    return listen(DefaultListen(), port, requireall, servertype);
}

bool ServerPool::bind(QList<QHostAddress> addrs, quint16 port,
                      bool requireall)
{
    m_port = port;
    for (const auto & qha : std::as_const(addrs))
    {
        // If IPV4 support is disabled and this is an IPV4 address,
        // bypass this address
        if (qha.protocol() == QAbstractSocket::IPv4Protocol
          && ! gCoreContext->GetBoolSetting("IPv4Support",true))
            continue;
        // If IPV6 support is disabled and this is an IPV6 address,
        // bypass this address
        if (qha.protocol() == QAbstractSocket::IPv6Protocol
          && ! gCoreContext->GetBoolSetting("IPv6Support",true))
            continue;

        QNetworkAddressEntry host;
        QNetworkAddressEntry wildcard;

        if (qha.protocol() == QAbstractSocket::IPv6Protocol)
        {
            for (const auto& iae : std::as_const(naList_6))
            {
                if (PrivUdpSocket::contains(iae, qha))
                {
                    host = iae;
                    break;
                }
                if (iae.ip() == QHostAddress::AnyIPv6)
                    wildcard = iae;
            }
        }
        else
        {
            for (const auto& iae : std::as_const(naList_4))
            {
                if (PrivUdpSocket::contains(iae, qha))
                {
                    host = iae;
                    break;
                }
                if (iae.ip() == QHostAddress::AnyIPv4)
                    wildcard = iae;
            }
        }

        if (host.ip().isNull())
        {
            if (wildcard.ip().isNull())
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed to find local address to use for destination %1:%2.")
                    .arg(prettyip(qha)).arg(port));
                continue;
            }

            LOG(VB_GENERAL, LOG_DEBUG,
                QString("Failed to find local address to use for destination %1:%2. Using wildcard.")
                .arg(prettyip(qha)).arg(port));
            host = wildcard;
        }

        auto *socket = new PrivUdpSocket(this, host);

        if (socket->bind(qha, port))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Binding to UDP %1:%2")
                    .arg(prettyip(qha)).arg(port));
            m_udpSockets.append(socket);
            connect(socket, &QIODevice::readyRead,
                    this,   &ServerPool::newUdpDatagram);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed binding to UDP %1:%2 - Error %3: %4")
                        .arg(prettyip(qha))
                        .arg(port)
                        .arg(socket->error())
                        .arg(socket->errorString()));
            socket->disconnect();
            socket->deleteLater();

            if (socket->error() == QAbstractSocket::SocketAddressNotAvailableError)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Address %1 no longer exists - ignoring")
                    .arg(prettyip(qha)));
                continue;
            }

            if (requireall)
            {
                close();
                return false;
            }
        }
    }

    if (m_udpSockets.empty())
        return false;

    m_listening = true;
    return true;
}

bool ServerPool::bind(QStringList addrstr, quint16 port, bool requireall)
{
    QList<QHostAddress> addrs;
    for (const auto & str : std::as_const(addrstr))
        addrs << QHostAddress(str);
    return bind(addrs, port, requireall);
}

bool ServerPool::bind(quint16 port, bool requireall)
{
    return bind(DefaultListen(), port, requireall);
}

qint64 ServerPool::writeDatagram(const char * data, qint64 size,
                                 const QHostAddress &addr, quint16 port)
{
    if (!m_listening || m_udpSockets.empty())
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
    {
        // Couldn't find an exact socket. See is there is a wildcard one.
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("No exact socket match for %1:%2. Searching for wildcard.")
            .arg(prettyip(addr)).arg(port));
        for (auto *val : std::as_const(m_udpSockets))
        {
            if ((addr.protocol() == QAbstractSocket::IPv6Protocol &&
                 val->host().ip() == QHostAddress::AnyIPv6) ||
                (val->host().ip() == QHostAddress::AnyIPv4))
            {
                m_lastUdpSocket = val;
                break;
            }
        }
    }
    if (!m_lastUdpSocket)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("No m_lastUdpSocket"));
        return -1;
    }

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

void ServerPool::newTcpConnection(qintptr socket)
{
    // Ignore connections from an SSL server for now, these are only handled
    // by HttpServer which overrides newTcpConnection
    auto *server = qobject_cast<PrivTcpServer *>(QObject::sender());
    if (!server || server->GetServerType() == kSSLServer)
        return;

    auto *qsock = new QTcpSocket(this);
    if (qsock->setSocketDescriptor(socket)
       && gCoreContext->CheckSubnet(qsock))
    {
        emit newConnection(qsock);
    }
    else
    {
        delete qsock;
    }
}

void ServerPool::newUdpDatagram(void)
{
    auto *socket = qobject_cast<QUdpSocket*>(sender());

    while (socket->state() == QAbstractSocket::BoundState &&
           socket->hasPendingDatagrams())
    {
        QByteArray buffer;
        buffer.resize(socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort = 0;

        socket->readDatagram(buffer.data(), buffer.size(),
                             &sender, &senderPort);
        if (gCoreContext->CheckSubnet(sender))
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
 * isipv6:   is set to true if IPv6 was successful (default nullptr)
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

            // did we fail because IPv6 isn't available?
            QAbstractSocket::SocketError err = server->serverError();
            if (err == QAbstractSocket::UnsupportedSocketOperationError)
            {
                ipv6 = false;
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
 * isipv6:   is set to true if IPv6 was successful (default nullptr)
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

            // did we fail because IPv6 isn't available?
            QAbstractSocket::SocketError err = socket->error();
            if (err == QAbstractSocket::UnsupportedSocketOperationError)
            {
                ipv6 = false;
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
