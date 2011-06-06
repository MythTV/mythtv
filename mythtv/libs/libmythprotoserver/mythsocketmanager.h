#ifndef _MYTHSOCKETMANAGER_H_
#define _MYTHSOCKETMANAGER_H_

using namespace std;

// Qt
#include <QReadWriteLock>
#include <QMap>
#include <QList>
#include <QTimer>
#include <QWaitCondition>
#include <QTcpServer>

// MythTV
#include "mythsocket.h"
#include "mythdeque.h"
#include "socketrequesthandler.h"
#include "sockethandler.h"

class ProcessRequestThread;
class MythSocketManager;

class MythServer : public QTcpServer
{
    Q_OBJECT

  signals:
    void newConnect(MythSocket*);

  protected:
    virtual void incomingConnection(int socket);
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
    void MarkUnused(ProcessRequestThread *prt);

    void AddSocketHandler(SocketHandler *socket);
    SocketHandler *GetConnectionBySocket(MythSocket *socket);

    void ProcessRequest(MythSocket *socket);

    void RegisterHandler(SocketRequestHandler *handler);
    bool Listen(int port);

  public slots:
    void newConnection(MythSocket *socket) { socket->setCallbacks(this); }

  private:
    void ProcessRequestWork(MythSocket *socket);
    void HandleVersion(MythSocket *socket, const QStringList slist);
    void HandleDone(MythSocket *socket);

    MythDeque<ProcessRequestThread *>   m_threadPool;
    QMutex                              m_threadPoolLock;
    QWaitCondition                      m_threadPoolCond;

    QMap<MythSocket*, SocketHandler*>   m_socketMap;
    QReadWriteLock                      m_socketLock;

    QMap<QString, SocketRequestHandler*>    m_handlerMap;
    QReadWriteLock                          m_handlerLock;

    MythServer     *m_server;
};
#endif
