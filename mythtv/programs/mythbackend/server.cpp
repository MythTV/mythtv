#include "server.h"
#include "mythlogging.h"

void MythServer::newTcpConnection(int socket)
{
    emit NewConnection(socket);
}

