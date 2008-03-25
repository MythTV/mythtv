#ifndef SERVER_H_
#define SERVER_H_

#include <q3serversocket.h>
#include <qmutex.h>

class MythSocket;

class MythServer : public Q3ServerSocket
{
    Q_OBJECT
  public:
    MythServer(int port, QObject *parent = 0);

    void newConnection(int socket);

  signals:
    void newConnect(MythSocket *);
};


#endif
