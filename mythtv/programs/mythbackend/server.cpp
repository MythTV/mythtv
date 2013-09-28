#include "server.h"
#include "mythlogging.h"

void MythServer::newTcpConnection(qt_socket_fd_t socket)
{
    emit NewConnection(socket);
}

