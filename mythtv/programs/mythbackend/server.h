#ifndef SERVER_H_
#define SERVER_H_

#include "serverpool.h"

class MythSocket;

class MythServer : public ServerPool
{
    Q_OBJECT

  signals:
    void NewConnection(qt_socket_fd_t socketDescriptor);

  protected:
    void newTcpConnection(qt_socket_fd_t socket) override; // ServerPool
};


#endif
