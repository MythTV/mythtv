/*
	serversocket.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for the lcd server side socket connections

*/

#include <stdlib.h>
#include <iostream>
using namespace std;

#include "serversocket.h"


LCDServerSocket::LCDServerSocket(int port, QObject *parent)
                :Q3ServerSocket(port, 1, parent)
{
    if (!ok())
    {
        cerr << "LCDServerSocket.o: Something is wrong trying to start with port=" << port << endl;
        cerr << "LCDServerSocket.o: You've probably got another copy of lcd server already running." << endl;
        cerr << "LCDServerSocket.o: You might want to try \"ps ax | grep mythlcdserver\"." << endl ; 
        cerr << "LCDServerSocker.o: This copy will now exit ... " << endl ;
        exit(0);
    }
}


void LCDServerSocket::newConnection(int socket)
{
    Q3Socket *new_socket = new Q3Socket(this);
    connect(new_socket, SIGNAL(delayedCloseFinished()), this, SLOT(discardClient()));
    connect(new_socket, SIGNAL(connectionClosed()), this, SLOT(discardClient()));
    new_socket->setSocket(socket);
    
    emit(newConnect(new_socket));
}

void LCDServerSocket::discardClient()
{
    Q3Socket *socket = (Q3Socket *)sender();
    if (socket)
    {
        emit(endConnect(socket));
    }
}

