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

#ifndef LIB_HTTPD_SELECT_H

#define LIB_HTTPD_SELECT_H 1

#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <iostream>
#include <strings.h>

#include <vector>
#include <list>
#include <algorithm>

#include "httpd.h"
#include "httpd_priv.h"

#ifdef __APPLE__
	typedef int socklen_t;
#endif

#ifdef __sgi__
	typedef int socklen_t;
#endif

#define min(a, b)       (a) < (b) ? (a) : (b)
#define find_(a, b)	find( (a.begin()), (a.end()), (b) )
#define erase_(a, b, c)	a.erase( (a.begin() + b), (a.begin() + c) )
#define READBUFSIZE 10240

typedef std::vector<char> BufferType;
typedef BufferType::iterator BufferIterator;

struct Client {
	int fDesc;
	char address[17];
	int finished;
	int bytesSent;
	BufferType readBuffer;
	BufferType writeBuffer; 
};

struct httpd;

class Clients {
protected:
	std::list<Client> clientList;
	typedef std::list<Client>::iterator ClientIterator;

	ClientIterator locateFDesc( const int fDesc ) {
		ClientIterator c = clientList.begin();
		while (c != clientList.end()) {
			if (c->fDesc == fDesc)
				return c;
			c++;
		}
	
		return 0;
	}


public:	
	int calcFDWrite(fd_set &fd) {
		ClientIterator c;
		int count = 0;
		
		c = clientList.begin();
		while (c != clientList.end()) {
			if (c->writeBuffer.size() > 0) {
				FD_SET( c->fDesc, &fd );
				count++;
				c++;
			} else {
				if (c->finished) {
					close(c->fDesc);
					clientList.erase(c);
					c = clientList.begin();
				} else {
					c++;
				}
			}
		} 
		return count;
	}
	
	void calcFDRead(fd_set &fd) {
		ClientIterator c;
		
		c = clientList.begin();
		while (c != clientList.end()) {
			FD_SET( c->fDesc, &fd );
			c++;
		} 
	}

	int getMaxSocket() {
		int maxSocket = 0;
		ClientIterator c;
		c = clientList.begin();
		while (c != clientList.end()) {
			if ( (c->fDesc) && (c->fDesc > maxSocket) )
				maxSocket = c->fDesc;
			c++;
		}
		return maxSocket;
	}

	int getMinSocket() {
		int minSocket = 0;
		ClientIterator c;
		c = clientList.begin();
		while (c != clientList.end()) {
			if ( (c->fDesc) && (c->fDesc < minSocket) )
				minSocket = c->fDesc;
			c++;
		}
		return minSocket;
	}
	
	void clearWBuffers() {
		ClientIterator c;
		c = clientList.begin();
		while (c != clientList.end()) {
			c->writeBuffer.clear(); 
			c++;
		}
	}
	
	void erase( const int fDesc ) {
		ClientIterator c;
		if ((c = locateFDesc(fDesc)) != 0) {
			clientList.erase(c);
			close(fDesc);
		}
	}

	void finish( const int fDesc ) {
		ClientIterator c;
		if ((c = locateFDesc(fDesc)) != 0) {
			c->finished = true;
		}
	}

	void address( const int fDesc, char address[HTTP_IP_ADDR_LEN] ) {
		ClientIterator c;
		if ((c = locateFDesc(fDesc)) != 0) {
			strncpy(address, c->address, HTTP_IP_ADDR_LEN);
		}
	}
	
	int getNumClients() {
		return clientList.size();
	}

	int readBuf(const int fDesc, char *destBuf, const uint len) {
		ClientIterator c;
		if ((c = locateFDesc(fDesc)) == 0) {
			// printf("unknown client id %d\n", fDesc);
			return 0;
		}
	
		memcpy( destBuf, &(c->readBuffer[0]), min( len, c->readBuffer.size() ) );
		erase_(c->readBuffer, 0, (min( len, c->readBuffer.size()) - 1 ));
		return 1;
	}
		
	int readLine(const int fDesc, char *destBuf, const uint len) {
		ClientIterator c;
		if ((c = locateFDesc(fDesc)) == 0) {
			// printf("unknown client id %d\n", fDesc);
			return 0;
		}
		BufferIterator lineEnd;

		if((lineEnd = std::find_(c->readBuffer, '\n')) != c->readBuffer.end()) {
			if (*(lineEnd - 1) == '\r') {
				c->readBuffer.erase(lineEnd - 1);
				lineEnd--;
			}
			unsigned int lineLength = (uint) min( ((uint) len), (uint) (&(*lineEnd) - &(c->readBuffer[0])) );
			
			memcpy( destBuf, &(c->readBuffer[0]), lineLength + 1 );
			destBuf[lineLength] = 0;
			erase_(c->readBuffer, 0, lineLength + 1);
			return 1;
		} else {
			return 0;
		}
	}

	int handleWrite(int socket) {
		int bytesWritten;
		ClientIterator c;
		if ((c = locateFDesc(socket)) == 0) {
			// printf("unknown client id %d\n", socket);
			return 2;
		}

		if ( (bytesWritten = send(socket, &(c->writeBuffer[c->bytesSent]), c->writeBuffer.size() - c->bytesSent, 0)) <= 0) {
			// printf("lost connection to client\n");
			return 1;
		} else {
			c->bytesSent += bytesWritten;
			if ( (unsigned int) c->bytesSent == c->writeBuffer.size())
			{
				// all done
				c->bytesSent = 0;
				c->writeBuffer.clear();
			}
			return 0;
		}
	} 
		
	int handleExisting( const int fDesc ) {	
		char buffer[READBUFSIZE];	// Buffer for socket reads
		int bytesRead;
	
		bzero(buffer, READBUFSIZE);
	
		if ( (bytesRead = recv(fDesc, buffer, READBUFSIZE, 0)) <= 0) {
			// printf("lost connection to client\n");
			return 1;
		} else {
			ClientIterator c;
			if ((c = locateFDesc(fDesc)) == 0) {
				// printf("unknown client id %d\n", fDesc);
				return 2;
			}
			// append new data to read buffer
			c->readBuffer.insert(c->readBuffer.end(), buffer, buffer + bytesRead);

			return 0;
		}
	}

	int handleNew(httpd *server) {
		int newSocket;
		struct  sockaddr_in     addr;
		size_t  addrLen;
		char	*ipaddr;
	
		bzero(&addr, sizeof(addr));
		addrLen = sizeof(addr);

		if ( (newSocket = accept(server->serverSock,(struct sockaddr *)&addr, (socklen_t *)&addrLen)) < 0 )
			return -1;

		_httpd_setnonblocking(newSocket);

		Client c;
		c.fDesc = newSocket;
		c.finished = false;
		c.bytesSent = 0;
		
		ipaddr = inet_ntoa(addr.sin_addr);
		if (ipaddr)
			strncpy(c.address, ipaddr, HTTP_IP_ADDR_LEN);
		else
			bzero(&c.address, sizeof(c.address));

		clientList.push_back(c);
		return(newSocket);
	}
	
	void doWrite(const int fDesc, const char* string, const uint len) {
		ClientIterator c;
		if ((c = locateFDesc(fDesc)) == 0) {
			clientList.erase(c);
			close(fDesc);
		}
	
		c->writeBuffer.insert( c->writeBuffer.end(), string, string + len );
	}

	inline void doWrite(Client *c, const char* string, const uint len) {
		c->writeBuffer.insert( c->writeBuffer.end(), string, string + len );
	}

	void doWrite(const int fDesc, const char* string) {
		ClientIterator c;
		if ((c = locateFDesc(fDesc)) == 0) {
			clientList.erase(c);
			close(fDesc);
		}

		c->writeBuffer.insert( c->writeBuffer.end(), string, string+strlen(string)-1 );
	}

	inline void doWrite(Client *c, const char* string) {
		c->writeBuffer.insert( c->writeBuffer.end(), string, string+strlen(string)-1 );
	}

	void doWrite(const char* string) {
		ClientIterator c;
		c = clientList.begin();
		while (c != clientList.end()) {
			c->writeBuffer.insert( c->writeBuffer.end(), string, string+strlen(string) );
			c++;
		}
	}

};


void handleSelectError( httpd *server );
void handleSelectTimeout( httpd *server, const int fDesc ); 
void handleSelectRead( httpd *server, const int fDesc ); 
void handleSelectWrite( httpd *server, const int fDesc ); 

int getMinSocket(httpd *server);
int getMaxSocket(httpd *server);
void calcFDRead(httpd *server, fd_set &fdRead); 
int calcFDWrite(fd_set &fdWrite);

int httpdSelectLoop( httpd *server, struct timeval timeout);

#endif
