#include <iostream>
using namespace std;

#include "server.h"

MythServer::MythServer(int port, QObject *parent)
          : QServerSocket(port, 1, parent)
{
    if (!ok())
    {
        cerr << "Failed to bind port: " << port << endl;
        exit(0);
    }
}

void MythServer::newConnection(int socket)
{
    QSocket *s = new QSocket(this);
    connect(s, SIGNAL(delayedCloseFinished()), this, SLOT(discardClient()));
    connect(s, SIGNAL(connectionClosed()), this, SLOT(discardClient()));
    s->setSocket(socket);

    emit(newConnect(s)); 
}

void MythServer::discardClient(void)
{
    QSocket *socket = (QSocket *)sender();
    emit(endConnect(socket));
    delete socket;
}
