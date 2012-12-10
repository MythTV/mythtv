#ifndef _MYTHSOCKETMANAGER_H_
#define _MYTHSOCKETMANAGER_H_

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
    MythServer(QObject *parent=0);

  signals:
    void newConnection(int socketDescriptor);

  protected slots:
    virtual void newTcpConnection(int socket);
};

class PROTOSERVER_PUBLIC MythSocketManager : public QObject, public MythSocketCBs
{
    Q_OBJECT
  public:
    MythSocketManager();
   ~MythSocketManager();

    void readyRead(MythSocket *socket);
    void connectionClosed(MythSocket *socket);
    void connectionFailed(MythSocket *socket) { (void)socket; }
    void connected(MythSocket *socket) { (void)socket; }

    void SetThreadCount(uint count);

    void AddSocketHandler(SocketHandler *socket);
    SocketHandler *GetConnectionBySocket(MythSocket *socket);

    void ProcessRequest(MythSocket *socket);

    void RegisterHandler(SocketRequestHandler *handler);
    bool Listen(int port);

  public slots:
    void newConnection(int sd);

  private:
    void ProcessRequestWork(MythSocket *socket);
    void HandleVersion(MythSocket *socket, const QStringList slist);
    void HandleDone(MythSocket *socket);

    QMap<MythSocket*, SocketHandler*>   m_socketMap;
    QReadWriteLock                      m_socketLock;

    QMap<QString, SocketRequestHandler*>    m_handlerMap;
    QReadWriteLock                          m_handlerLock;

    MythServer     *m_server;
    MThreadPool     m_threadPool;

    QMutex m_socketListLock;
    QSet<MythSocket*> m_socketList;
};
#endif
