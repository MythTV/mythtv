/*
** Copyright (c) 2002  Hughes Technologies Pty Ltd.  All rights
** reserved.
**
** Copyright (c) 2003  deleet, Alexander Oberdoerster
**
** Terms under which this software may be used or copied are
** provided in the  specific license associated with this product.
**
** Hughes Technologies disclaims all warranties with regard to this
** software, including all implied warranties of merchantability and
** fitness, in no event shall Hughes Technologies be liable for any
** special, indirect or consequential damages or any damages whatsoever
** resulting from loss of use, data or profits, whether in an action of
** contract, negligence or other tortious action, arising out of or in
** connection with the use or performance of this software.
**
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <string.h>
#include <dirent.h>

#include <string>
#include <deque>
#include <list>
#include <algorithm>
#include <cctype>
#include <iostream>

#include "httpd.h"
#include "httpd_priv.h"
#include "select.h"


////////
// globals

int gFinished = 0;
int gReturn = 0;


////////
// select arbitration

void handleSelectError( httpd *server ) 
{
	perror( "(select)" );
	switch (errno) {			
	// ignore interrupts
	case EINTR:
		break;
	// die otherwise
	default:
		server->clients->clearWBuffers();
		gFinished = 1;
		gReturn = -1;
	}
}

void handleSelectTimeout( httpd *server, const int fDesc ) 
{
	server->clients->erase(fDesc);
}

void handleSelectRead( httpd *server, const int fDesc ) 
{
	if(fDesc == server->serverSock) {
		if ( server->clients->handleNew(server) < 0 )  
			perror( "(accept)" );
	} else {
		if (server->clients->handleExisting(fDesc) > 0) {
			server->clients->erase(fDesc);
		} else {
			server->clientSock = fDesc;
                        server->clients->address(fDesc, server->clientAddr);
			if (httpdReadRequest(server) < 0) {
				httpdEndRequest(server);
			} else {
				httpdProcessRequest(server); 
				if (server->request.version < 1.1 || server->request.close) {
					httpdEndRequest(server);
				} else {
					httpdSuspendRequest(server);
				}
			}
              }
	}
}

void handleSelectWrite( httpd *server, const int fDesc ) 
{
	if (server->clients->handleWrite(fDesc) > 1) 
		server->clients->erase(fDesc);
}


////////
// select utility functions

int getMinSocket(httpd *server)
{
	int minSocket;
	
	minSocket = (server->serverSock > server->clients->getMinSocket()) ?  
		server->serverSock : server->clients->getMinSocket();
	return minSocket;
}

int getMaxSocket(httpd *server)
{
	int maxSocket;
	
	maxSocket = (server->serverSock > server->clients->getMaxSocket()) ?  
		server->serverSock : server->clients->getMaxSocket();
	return maxSocket;
}

void calcFDRead(httpd *server, fd_set &fdRead) 
{
	FD_ZERO( &fdRead );
	FD_SET( server->serverSock, &fdRead );
	server->clients->calcFDRead(fdRead);
}

int calcFDWrite(httpd *server, fd_set &fdWrite) 
{
	int pendingWrites = 0;

	FD_ZERO( &fdWrite );
	pendingWrites += server->clients->calcFDWrite(fdWrite);
	return pendingWrites;
}


int httpdSelectLoop( httpd *server, struct timeval& timeout_ ) {
	timeval timeout;

	// autoreap children
	signal(SIGCHLD, SIG_IGN);

	fd_set fdRead, fdWrite;
	int minSocket, maxSocket;
	int pendingWrites = 0;
	int ret;
		
	FD_ZERO(&fdWrite);
	
	while( !gFinished || (pendingWrites != 0) ) {
		timeout = timeout_;

		calcFDRead(server, fdRead);
		minSocket = getMinSocket(server);
		maxSocket = getMaxSocket(server);

		ret = select( maxSocket+1, &fdRead, &fdWrite, NULL, &timeout );

		if ( ret < 0 ) {
			// select returned an error
			handleSelectError( server );
		} else if (ret == 0) {
			// timeout hit
			for( int i = minSocket; i <= maxSocket; i++ ) 
				if( FD_ISSET( i, &fdWrite ))
					handleSelectTimeout(server, i);
		} else {
			// handle i/o
			for( int i = minSocket; i <= maxSocket; i++ ) {
				if( FD_ISSET( i, &fdRead )) {
					handleSelectRead(server, i);
				}
				if( FD_ISSET( i, &fdWrite )) {
					handleSelectWrite(server, i);
				}
			}
		}

		pendingWrites = calcFDWrite(server, fdWrite);
	}

	return gReturn;
}
