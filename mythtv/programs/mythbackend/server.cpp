#include "server.h"
#include "mythsocket.h"

void MythServer::incomingConnection(int socket)
{
    MythSocket *s = new MythSocket(socket);
    emit newConnect(s);
}

