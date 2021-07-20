#ifndef MYTHSOCKETMANAGER_H
#define MYTHSOCKETMANAGER_H

// Qt
#include <QMap>
#include <QSet>
#include <QList>
#include <QMutex>
#include <QTimer>
#include <QReadWriteLock>
#include <QWaitCondition>

// MythTV
#include "socketrequesthandler.h"
#include "sockethandler.h"
#include "mthreadpool.h"
#include "mythsocket.h"
#include "serverpool.h"

class MythServer : public ServerPool
{
    Q_OBJECT
  public:
    explicit MythServer(QObject *parent=nullptr);

  signals:
    void newConnection(qintptr socket);

  protected slots:
    void newTcpConnection(qintptr socket) override; // ServerPool
};

class PROTOSERVER_PUBLIC MythSocketManager : public QObject, public MythSocketCBs
{
    Q_OBJECT
  public:
    MythSocketManager();
   ~MythSocketManager() override;

    void readyRead(MythSocket *socket) override; // MythSocketCBs
    void connectionClosed(MythSocket *socket) override; // MythSocketCBs
    void connectionFailed(MythSocket *socket) override // MythSocketCBs
        { (void)socket; }
    void connected(MythSocket *socket) override // MythSocketCBs
        { (void)socket; }

    void SetThreadCount(uint count);

    void AddSocketHandler(SocketHandler *socket);
    SocketHandler *GetConnectionBySocket(MythSocket *socket);

    void ProcessRequest(MythSocket *socket);

    void RegisterHandler(SocketRequestHandler *handler);
    bool Listen(int port);

  public slots:
    void newConnection(qintptr sd);

  private:
    void ProcessRequestWork(MythSocket *socket);
    static void HandleVersion(MythSocket *socket, const QStringList &slist);
    static void HandleDone(MythSocket *socket);

    QMap<MythSocket*, SocketHandler*>   m_socketMap;
    QReadWriteLock                      m_socketLock;

    QMap<QString, SocketRequestHandler*>    m_handlerMap;
    QReadWriteLock                          m_handlerLock;

    MythServer     *m_server     { nullptr };
    MThreadPool     m_threadPool;

    QMutex m_socketListLock;
    QSet<MythSocket*> m_socketList;
};
#endif // MYTHSOCKETMANAGER_H
