#include <iostream>
#include <cstdlib>
using namespace std;

#include "server.h"
#include "mythsocket.h"

MythServer::MythServer(int port, QObject *parent)
          : Q3ServerSocket(port, 1, parent)
{
}

void MythServer::newConnection(int socket)
{
    MythSocket *s = new MythSocket(socket);
    emit(newConnect(s)); 
}

