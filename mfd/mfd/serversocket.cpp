/*
	serversocket.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for the mfd server side socket connections

*/

#include <stdlib.h>
#include <iostream>
using namespace std;

#include "serversocket.h"


MFDClientSocket::MFDClientSocket(int identifier, QObject* parent=0, const char* name=0)
                :QSocket(parent, name)
{
    unique_identifier = identifier;
}

int MFDClientSocket::getIdentifier()
{
    return unique_identifier;
}

/*
---------------------------------------------------------------------
*/

MFDServerSocket::MFDServerSocket(int port, QObject *parent)
                :QServerSocket(port, 1, parent)
{
    client_identifiers = 0;
    if(!ok())
    {
        cerr << "MFDServerSocket.o: Something is wrong trying to start with port=" << port << endl;
        cerr << "MFDServerSocket.o: You've probably got another copy of mfd already running." << endl;
        cerr << "MFDServerSocket.o: You might want to try \"ps ax | grep mfd\"." << endl ; 
        cerr << "MFDServerSocker.o: This copy will now exit ... " << endl ;
        exit(0);
    }
}

int MFDServerSocket::bumpClientIdentifiers()
{
    ++client_identifiers;
    return client_identifiers;
}

void MFDServerSocket::newConnection(int socket)
{
    MFDClientSocket *new_socket = new MFDClientSocket(bumpClientIdentifiers(), this);
    connect(new_socket, SIGNAL(delayedCloseFinished()), this, SLOT(discardClient()));
    connect(new_socket, SIGNAL(connectionClosed()), this, SLOT(discardClient()));
    new_socket->setSocket(socket);
    
    emit(newConnect(new_socket));
}

void MFDServerSocket::discardClient()
{
    MFDClientSocket *socket = (MFDClientSocket *)sender();
    if(socket)
    {
        emit(endConnect(socket));        delete socket;
    }
}

