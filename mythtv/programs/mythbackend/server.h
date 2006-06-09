#ifndef SERVER_H_
#define SERVER_H_

#include <qserversocket.h>
#include <qmutex.h>

class MythSocket;

class MythServer : public QServerSocket
{
    Q_OBJECT
  public:
    MythServer(int port, QObject *parent = 0);

    void newConnection(int socket);

  signals:
    void newConnect(MythSocket *);
};


#endif
