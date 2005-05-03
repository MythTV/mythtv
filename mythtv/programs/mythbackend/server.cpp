#include <iostream>
#include <cstdlib>
using namespace std;

#include "server.h"

MythServer::MythServer(int port, QObject *parent)
          : QServerSocket(port, 1, parent)
{
}

void MythServer::newConnection(int socket)
{
    RefSocket *s = new RefSocket(this);
    connect(s, SIGNAL(delayedCloseFinished()), this, SLOT(discardClient()));
    connect(s, SIGNAL(connectionClosed()), this, SLOT(discardClient()));
    s->setSocket(socket);

    emit(newConnect(s)); 
}

void MythServer::discardClient(void)
{
    RefSocket *socket = (RefSocket *)sender();
    if (socket)
    {
        emit(endConnect(socket));
    }
}
