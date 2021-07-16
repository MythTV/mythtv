#ifndef SERVER_H_
#define SERVER_H_

#include "serverpool.h"

class MythSocket;

class MythServer : public ServerPool
{
    Q_OBJECT

  signals:
    void NewConnection(qintptr socketDescriptor);

  protected:
    void newTcpConnection(qintptr socket) override; // ServerPool
};


#endif
