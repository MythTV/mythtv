#ifndef SERVER_H_
#define SERVER_H_

#include "serverpool.h"

class MythSocket;

class MythServer : public ServerPool
{
    Q_OBJECT

  signals:
    void newConnect(MythSocket*);

  protected:
    virtual void newTcpConnection(int socket);
};


#endif
