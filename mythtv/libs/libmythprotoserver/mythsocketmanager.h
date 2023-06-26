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
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythsocket.h"
#include "libmythbase/serverpool.h"

#include "sockethandler.h"
#include "socketrequesthandler.h"

class PROTOSERVER_PUBLIC MythSocketManager : public QObject, public MythSocketCBs
{
    Q_OBJECT
  public:
    MythSocketManager();
   ~MythSocketManager() override;

    void readyRead(MythSocket *socket) override; // MythSocketCBs
    void connectionClosed(MythSocket *socket) override; // MythSocketCBs
    void connectionFailed([[maybe_unused]] MythSocket *socket) override {}; // MythSocketCBs
    void connected([[maybe_unused]] MythSocket *socket) override {}; // MythSocketCBs

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
