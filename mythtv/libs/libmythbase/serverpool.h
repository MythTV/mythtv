#ifndef SERVERPOOL_H_
#define SERVERPOOL_H_

#include <QList>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QSslSocket>
#include <QUdpSocket>
#include <QStringList>

#include "mythbaseexp.h"

/** \class ServerPool
 *  \brief Manages a collection of sockets listening on different ports.
 *
 *  This class allows a TCP or UDP server to listen on a list of addresses
 *  rather than limited to a single or all addresses. This is done by opening
 *  a separate server for each defined QHostAddress, and signalling
 *  collectively for any new connections.
 *
 *  This can be subclassed with new 'newTcpConnection()' and 'newConnection()'
 *  methods to allow signalling for alternate socket types.  For a minimal
 *  example see MythServer.
 */

class PrivUdpSocket;

enum PoolServerType : std::uint8_t
{
    kTCPServer,
    kUDPServer,
    kSSLServer
};

// Making a 'Priv' server public is a contradiction, it was this or passing
// through the server type in the newConnection signal which would have required
// modifying a lot more places to handle or ignore the info
class MBASE_PUBLIC PrivTcpServer : public QTcpServer
{
    Q_OBJECT
  public:
    explicit PrivTcpServer(QObject *parent = nullptr,
                  PoolServerType type = kTCPServer);
   ~PrivTcpServer() override = default;

   PoolServerType GetServerType(void) { return m_serverType; }

  signals:
    void newConnection(qintptr socket);

  protected:
    void incomingConnection(qintptr socket) override; // QTcpServer

  private:
    PoolServerType m_serverType;
};

class MBASE_PUBLIC ServerPool : public QObject
{
    Q_OBJECT

  public:
    explicit ServerPool(QObject *parent=nullptr)
        : QObject(parent) {}
   ~ServerPool(void) override;

    static void RefreshDefaultListen(void);
    static QList<QHostAddress> DefaultListen(void);
    static QList<QHostAddress> DefaultListenIPv4(void);
    static QList<QHostAddress> DefaultListenIPv6(void);
    static QList<QHostAddress> DefaultBroadcast(void);
    static QList<QHostAddress> DefaultBroadcastIPv4(void);
    static QList<QHostAddress> DefaultBroadcastIPv6(void);

    bool listen(QList<QHostAddress> addrs, quint16 port, bool requireall=true,
                PoolServerType type = kTCPServer);
    bool listen(QStringList addrs, quint16 port, bool requireall=true,
                PoolServerType type = kTCPServer);
    bool listen(quint16 port, bool requireall=true,
                PoolServerType type = kTCPServer);

    bool bind(QList<QHostAddress> addrs, quint16 port, bool requireall=true);
    bool bind(QStringList addrs, quint16 port, bool requireall=true);
    bool bind(quint16 port, bool requireall=true);

    qint64 writeDatagram(const char * data, qint64 size,
                         const QHostAddress &addr, quint16 port);
    qint64 writeDatagram(const QByteArray &datagram,
                         const QHostAddress &addr, quint16 port);

    bool isListening(void) const                { return m_listening;       }
    int  maxPendingConnections(void) const      { return m_maxPendingConn;  }
    void setMaxPendingConnections(int n)        { m_maxPendingConn = n;     }
    quint16 serverPort(void) const              { return m_port;            }

    QNetworkProxy proxy(void)                   { return m_proxy;           }
    void setProxy(const QNetworkProxy &proxy)   { m_proxy = proxy;          }

    void close(void);

    int tryListeningPort(int baseport, int range = 1);
    int tryBindingPort(int baseport, int range = 1);
    // Utility functions
    static int tryListeningPort(QTcpServer *server, int baseport,
                                int range = 1, bool *isipv6 = nullptr);
    static int tryBindingPort(QUdpSocket *socket, int baseport,
                              int range = 1, bool *isipv6 = nullptr);

  signals:
    void newConnection(QTcpSocket *);
    void newDatagram(QByteArray, QHostAddress, quint16);

  protected slots:
    virtual void newUdpDatagram(void);
    virtual void newTcpConnection(qintptr socket);

  private:
    static void SelectDefaultListen(bool force=false);

    bool            m_listening             {false};
    int             m_maxPendingConn        {30};
    quint16         m_port                  {0};
    QNetworkProxy   m_proxy                 {QNetworkProxy::NoProxy};

    QList<PrivTcpServer*>   m_tcpServers;
    QList<PrivUdpSocket*>   m_udpSockets;
    PrivUdpSocket          *m_lastUdpSocket {nullptr};
};

class MBASE_PUBLIC MythServer : public ServerPool
{
    Q_OBJECT
  public:
    explicit MythServer(QObject *parent = nullptr) : ServerPool(parent) {}

  signals:
    void newConnection(qintptr socket);

  protected slots:
    void newTcpConnection(qintptr socket) override // ServerPool
    {
        emit newConnection(socket);
    }
};

#endif
