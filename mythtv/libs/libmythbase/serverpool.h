#ifndef SERVERPOOL_H_
#define SERVERPOOL_H_

#include <QList>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
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
 *  methods to allow signalling for alternate socket types.
 */

class PrivTcpServer : public QTcpServer
{
    Q_OBJECT
  public:
    PrivTcpServer(QObject *parent = 0);
   ~PrivTcpServer() {};

  signals:
    void newConnection(int socket);

  protected:
    void incomingConnection(int socket);
};

class MBASE_PUBLIC ServerPool : public QObject
{
    Q_OBJECT

  public:
    ServerPool(QObject *parent=0);
   ~ServerPool(void);

    static void RefreshDefaultListen(void);
    static QList<QHostAddress> DefaultListen(void);
    static QList<QHostAddress> DefaultListenIPv4(void);
    static QList<QHostAddress> DefaultListenIPv6(void);
    static QList<QHostAddress> DefaultBroadcast(void);
    static QList<QHostAddress> DefaultBroadcastIPv4(void);
//    static QList<QHostAddress> DefaultBroadcastIPv6(void);

    bool listen(QList<QHostAddress> addrs, quint16 port, bool requireall=true);
    bool listen(QStringList addrs, quint16 port, bool requireall=true);
    bool listen(quint16 port, bool requireall=true);

    bool bind(QList<QHostAddress> addrs, quint16 port, bool requireall=true);
    bool bind(QStringList addrs, quint16 port, bool requireall=true);
    bool bind(quint16 port, bool requireall=true);

    qint64 writeDatagram(const char * data, qint64 size,
                         const QHostAddress &addr, quint16 port);
    qint64 writeDatagram(const QByteArray &datagram,
                         const QHostAddress &addr, quint16 port);

    bool isListening(void)                      { return m_listening;       }
    int  maxPendingConnections(void)            { return m_maxPendingConn;  }
    void setMaxPendingConnections(int n)        { m_maxPendingConn = n;     }
    quint16 serverPort(void)                    { return m_port;            }

    QNetworkProxy proxy(void)                   { return m_proxy;           }
    void setProxy(const QNetworkProxy &proxy)   { m_proxy = proxy;          }

    void close(void);

  signals:
    void newConnection(QTcpSocket *);
    void newDatagram(QByteArray, QHostAddress, quint16);

  protected slots:
    virtual void newUdpDatagram(void);
    virtual void newTcpConnection(int socket);

  private:
    static void SelectDefaultListen(bool force=false);

    bool            m_listening;
    int             m_maxPendingConn;
    quint16         m_port;
    QNetworkProxy   m_proxy;

    QList<PrivTcpServer*>   m_tcpServers;
    QList<QUdpSocket*>      m_udpSockets;
};

#endif
