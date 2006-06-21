/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2005 Live Networks, Inc.  All rights reserved.
// An abstraction of a network interface used for RTP (or RTCP).
// (This allows the RTP-over-TCP hack (RFC 2326, section 10.12) to
// be implemented transparently.)
// Implementation

#include "RTPInterface.hh"
#include <GroupsockHelper.hh>
#include <stdio.h>

////////// Helper Functions - Definition //////////

// Helper routines and data structures, used to implement
// sending/receiving RTP/RTCP over a TCP socket:

static void sendRTPOverTCP(unsigned char* packet, unsigned packetSize,
			   int socketNum, unsigned char streamChannelId);

// Reading RTP-over-TCP is implemented using two levels of hash tables.
// The top-level hash table maps TCP socket numbers to a
// "SocketDescriptor" that contains a hash table for each of the
// sub-channels that are reading from this socket.

static HashTable* socketHashTable(UsageEnvironment& env) {
  _Tables* ourTables = _Tables::getOurTables(env);
  if (ourTables->socketTable == NULL) {
    // Create a new socket number -> SocketDescriptor mapping table:
    ourTables->socketTable = HashTable::create(ONE_WORD_HASH_KEYS);
  }
  return (HashTable*)(ourTables->socketTable);
}

class SocketDescriptor {
public:
  SocketDescriptor(UsageEnvironment& env, int socketNum);
  virtual ~SocketDescriptor();

  void registerRTPInterface(unsigned char streamChannelId,
			    RTPInterface* rtpInterface);
  RTPInterface* lookupRTPInterface(unsigned char streamChannelId);
  void deregisterRTPInterface(unsigned char streamChannelId);

private:
  static void tcpReadHandler(SocketDescriptor*, int mask);

private:
  UsageEnvironment& fEnv;
  int fOurSocketNum;
  HashTable* fSubChannelHashTable;
};

static SocketDescriptor* lookupSocketDescriptor(UsageEnvironment& env,
						int sockNum) {
  char const* key = (char const*)(long)sockNum;
  return (SocketDescriptor*)(socketHashTable(env)->Lookup(key));
}

static void removeSocketDescription(UsageEnvironment& env, int sockNum) {
  char const* key = (char const*)(long)sockNum;
  HashTable* table = socketHashTable(env);
  table->Remove(key);

  if (table->IsEmpty()) {
    // We can also delete the table (to reclaim space):
    _Tables* ourTables = _Tables::getOurTables(env);
    delete table;
    ourTables->socketTable = NULL;
    ourTables->reclaimIfPossible();
  }
}


////////// RTPInterface - Implementation //////////

RTPInterface::RTPInterface(Medium* owner, Groupsock* gs)
  : fOwner(owner), fGS(gs),
    fTCPStreams(NULL),
    fNextTCPReadSize(0), fNextTCPReadStreamSocketNum(-1),
    fReadHandlerProc(NULL),
    fAuxReadHandlerFunc(NULL), fAuxReadHandlerClientData(NULL) {
}

RTPInterface::~RTPInterface() {
  delete fTCPStreams;
}

Boolean RTPOverTCP_OK = True; // HACK: For detecting TCP socket failure externally #####

void RTPInterface::setStreamSocket(int sockNum,
				   unsigned char streamChannelId) {
  fGS->removeAllDestinations();
  addStreamSocket(sockNum, streamChannelId);
}

void RTPInterface::addStreamSocket(int sockNum,
				   unsigned char streamChannelId) {
  if (sockNum < 0) return;
  else RTPOverTCP_OK = True; //##### HACK

  for (tcpStreamRecord* streams = fTCPStreams; streams != NULL;
       streams = streams->fNext) {
    if (streams->fStreamSocketNum == sockNum
	&& streams->fStreamChannelId == streamChannelId) {
      return; // we already have it
    }
  }

  fTCPStreams = new tcpStreamRecord(sockNum, streamChannelId, fTCPStreams);
}

void RTPInterface::removeStreamSocket(int sockNum,
				      unsigned char streamChannelId) {
  for (tcpStreamRecord** streamsPtr = &fTCPStreams; *streamsPtr != NULL;
       streamsPtr = &((*streamsPtr)->fNext)) {
    if ((*streamsPtr)->fStreamSocketNum == sockNum
	&& (*streamsPtr)->fStreamChannelId == streamChannelId) {
      // Remove the record pointed to by *streamsPtr :
      tcpStreamRecord* next = (*streamsPtr)->fNext;
      (*streamsPtr)->fNext = NULL;
      delete (*streamsPtr);
      *streamsPtr = next;
      return;
    }
  }
}

void RTPInterface::sendPacket(unsigned char* packet, unsigned packetSize) {
  // Normal case: Send as a UDP packet:
  fGS->output(envir(), fGS->ttl(), packet, packetSize);

  // Also, send over each of our TCP socket:
  for (tcpStreamRecord* streams = fTCPStreams; streams != NULL;
       streams = streams->fNext) {
    sendRTPOverTCP(packet, packetSize,
		   streams->fStreamSocketNum, streams->fStreamChannelId);
  }
}

void RTPInterface
::startNetworkReading(TaskScheduler::BackgroundHandlerProc* handlerProc) {
  // Normal case: Arrange to read UDP packets:
  envir().taskScheduler().
    turnOnBackgroundReadHandling(fGS->socketNum(), handlerProc, fOwner);

  // Also, receive RTP over TCP, on each of our TCP connections:
  fReadHandlerProc = handlerProc;
  for (tcpStreamRecord* streams = fTCPStreams; streams != NULL;
       streams = streams->fNext) {
    // Get a socket descriptor for "streams->fStreamSocketNum":
    SocketDescriptor* socketDescriptor
      = lookupSocketDescriptor(envir(), streams->fStreamSocketNum);
    if (socketDescriptor == NULL) {
      socketDescriptor
	= new SocketDescriptor(envir(), streams->fStreamSocketNum);
      socketHashTable(envir())->Add((char const*)(long)(streams->fStreamSocketNum),
				    socketDescriptor);
    }

    // Tell it about our subChannel:
    socketDescriptor->registerRTPInterface(streams->fStreamChannelId, this);
  }
}

Boolean RTPInterface::handleRead(unsigned char* buffer,
				 unsigned bufferMaxSize,
				 unsigned& bytesRead,
				 struct sockaddr_in& fromAddress) {
  Boolean readSuccess;
  if (fNextTCPReadStreamSocketNum < 0) {
    // Normal case: read from the (datagram) 'groupsock':
    readSuccess = fGS->handleRead(buffer, bufferMaxSize, bytesRead, fromAddress);
  } else {
    // Read from the TCP connection:
    bytesRead = 0;
    unsigned totBytesToRead = fNextTCPReadSize;
    if (totBytesToRead > bufferMaxSize) totBytesToRead = bufferMaxSize; 
    unsigned curBytesToRead = totBytesToRead;
    unsigned curBytesRead;
    while ((curBytesRead = readSocket(envir(), fNextTCPReadStreamSocketNum,
				      &buffer[bytesRead], curBytesToRead,
				      fromAddress)) > 0) {
      bytesRead += curBytesRead;
      if (bytesRead >= totBytesToRead) break;
      curBytesToRead -= curBytesRead;
    }
    if (curBytesRead <= 0) {
      bytesRead = 0;
      readSuccess = False;
      RTPOverTCP_OK = False; // HACK #####
    } else {
      readSuccess = True;
    }
    fNextTCPReadStreamSocketNum = -1; // default, for next time
  }

  if (readSuccess && fAuxReadHandlerFunc != NULL) {
    // Also pass the newly-read packet data to our auxilliary handler:
    (*fAuxReadHandlerFunc)(fAuxReadHandlerClientData, buffer, bytesRead);
  }
  return readSuccess;
}

void RTPInterface::stopNetworkReading() {
  // Normal case
  envir().taskScheduler().turnOffBackgroundReadHandling(fGS->socketNum());

  // Also turn off read handling on each of our TCP connections:
  for (tcpStreamRecord* streams = fTCPStreams; streams != NULL;
       streams = streams->fNext) {
    SocketDescriptor* socketDescriptor
      = lookupSocketDescriptor(envir(), streams->fStreamSocketNum);
    if (socketDescriptor != NULL) {
      socketDescriptor->deregisterRTPInterface(streams->fStreamChannelId);
        // Note: This may delete "socketDescriptor",
        // if no more interfaces are using this socket
    }
  }
}


////////// Helper Functions - Implementation /////////

void sendRTPOverTCP(unsigned char* packet, unsigned packetSize,
                    int socketNum, unsigned char streamChannelId) {
#ifdef DEBUG
  fprintf(stderr, "sendRTPOverTCP: %d bytes over channel %d (socket %d)\n",
	  packetSize, streamChannelId, socketNum); fflush(stderr);
#endif
  // Send RTP over TCP, using the encoding defined in
  // RFC 2326, section 10.12:
  do {
    char const dollar = '$';
    if (send(socketNum, &dollar, 1, 0) != 1) break;
    if (send(socketNum, (char*)&streamChannelId, 1, 0) != 1) break;

    char netPacketSize[2];
    netPacketSize[0] = (char) ((packetSize&0xFF00)>>8);
    netPacketSize[1] = (char) (packetSize&0xFF);
    if (send(socketNum, netPacketSize, 2, 0) != 2) break;

    if (send(socketNum, (char*)packet, packetSize, 0) != (int)packetSize) break;

#ifdef DEBUG
    fprintf(stderr, "sendRTPOverTCP: completed\n"); fflush(stderr);
#endif

    return;
  } while (0);

  RTPOverTCP_OK = False; // HACK #####
#ifdef DEBUG
  fprintf(stderr, "sendRTPOverTCP: failed!\n"); fflush(stderr);
#endif
}

SocketDescriptor::SocketDescriptor(UsageEnvironment& env, int socketNum)
  : fEnv(env), fOurSocketNum(socketNum),
    fSubChannelHashTable(HashTable::create(ONE_WORD_HASH_KEYS)) {
}

SocketDescriptor::~SocketDescriptor() {
  delete fSubChannelHashTable;
}

void SocketDescriptor::registerRTPInterface(unsigned char streamChannelId,
					    RTPInterface* rtpInterface) {
  Boolean isFirstRegistration = fSubChannelHashTable->IsEmpty();
  fSubChannelHashTable->Add((char const*)(long)streamChannelId,
			    rtpInterface);

  if (isFirstRegistration) {
    // Arrange to handle reads on this TCP socket:
    TaskScheduler::BackgroundHandlerProc* handler
      = (TaskScheduler::BackgroundHandlerProc*)&tcpReadHandler;
    fEnv.taskScheduler().
      turnOnBackgroundReadHandling(fOurSocketNum, handler, this);
  }
}

RTPInterface* SocketDescriptor
::lookupRTPInterface(unsigned char streamChannelId) {
  char const* lookupArg = (char const*)(long)streamChannelId;
  return (RTPInterface*)(fSubChannelHashTable->Lookup(lookupArg));
}

void SocketDescriptor
::deregisterRTPInterface(unsigned char streamChannelId) {
  fSubChannelHashTable->Remove((char const*)(long)streamChannelId);

  if (fSubChannelHashTable->IsEmpty()) {
    // No more interfaces are using us, so it's curtains for us now
    fEnv.taskScheduler().turnOffBackgroundReadHandling(fOurSocketNum);
    removeSocketDescription(fEnv, fOurSocketNum);
    delete this;
  }
}

void SocketDescriptor::tcpReadHandler(SocketDescriptor* socketDescriptor,
				      int mask) {
  do {
    UsageEnvironment& env = socketDescriptor->fEnv; // abbrev
    int socketNum = socketDescriptor->fOurSocketNum;

    // Begin by reading and discarding any characters that aren't '$'.
    // Any such characters are probably regular RTSP responses or
    // commands from the server.  At present, we can't do anything with
    // these, because we have taken complete control of reading this socket.
    // (Later, fix) #####
    unsigned char c;
    struct sockaddr_in fromAddress;
    do {
      if (readSocket(env, socketNum, &c, 1, fromAddress) != 1) { // error reading TCP socket
	env.taskScheduler().turnOffBackgroundReadHandling(socketNum); // stops further calls to us
	return;
      }
    } while (c != '$');

    // The next byte is the stream channel id:
    unsigned char streamChannelId;
    if (readSocket(env, socketNum, &streamChannelId, 1, fromAddress)
	!= 1) break;
    RTPInterface* rtpInterface
      = socketDescriptor->lookupRTPInterface(streamChannelId);
    if (rtpInterface == NULL) break; // we're not interested in this channel

    // The next two bytes are the RTP or RTCP packet size (in network order)
    unsigned short size;
    if (readSocketExact(env, socketNum, (unsigned char*)&size, 2,
			fromAddress) != 2) break;
    rtpInterface->fNextTCPReadSize = ntohs(size);
    rtpInterface->fNextTCPReadStreamSocketNum = socketNum;
#ifdef DEBUG
    fprintf(stderr, "SocketDescriptor::tcpReadHandler() reading %d bytes on channel %d\n", rtpInterface->fNextTCPReadSize, streamChannelId);
#endif

    // Now that we have the data set up, call this subchannel's
    // read handler:
    if (rtpInterface->fReadHandlerProc != NULL) {
      rtpInterface->fReadHandlerProc(rtpInterface->fOwner, mask);
    }

  } while (0);
}


////////// tcpStreamRecord implementation //////////

tcpStreamRecord
::tcpStreamRecord(int streamSocketNum, unsigned char streamChannelId,
		  tcpStreamRecord* next)
  : fNext(next),
    fStreamSocketNum(streamSocketNum), fStreamChannelId(streamChannelId) {
}

tcpStreamRecord::~tcpStreamRecord() {
  delete fNext;
}
                                                                                
