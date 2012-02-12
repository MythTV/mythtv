#include "server.h"
#include "mythsocket.h"

void MythServer::newTcpConnection(int socket)
{
    MythSocket *s = new MythSocket(socket);
    emit newConnect(s);
}

