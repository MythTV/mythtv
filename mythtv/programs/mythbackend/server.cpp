#include "server.h"
#include "mythlogging.h"

void MythServer::newTcpConnection(qintptr socket)
{
    emit NewConnection(socket);
}
