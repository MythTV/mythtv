/*
	serversocket.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for the mtd server side socket connections

*/

#include <qsocket.h>

#include <stdlib.h>
#include <mythtv/mythcontext.h>

#include "serversocket.h"


MTDServerSocket::MTDServerSocket(int port, QObject *parent)
                :QServerSocket(port, 1, parent)
{
    if(!ok())
    {
        VERBOSE(VB_IMPORTANT,
                QString("MTDServerSocket Something is wrong trying to start "
                        "with port=%1\nYou've probably got another copy of mtd "
                        "already running.\nYou might want to try "
                        "\"ps ax | grep mtd\".\nThis copy will now exit ...")
                .arg(port));
        exit(0);
    }
}


void MTDServerSocket::newConnection(int socket)
{
    QSocket *new_socket = new QSocket(this);
    connect(new_socket, SIGNAL(delayedCloseFinished()), this, SLOT(discardClient()));
    connect(new_socket, SIGNAL(connectionClosed()), this, SLOT(discardClient()));
    new_socket->setSocket(socket);
    
    emit(newConnect(new_socket));
}

void MTDServerSocket::discardClient()
{
    QSocket *socket = (QSocket *)sender();
    if(socket)
    {
        emit(endConnect(socket));
    }
}

