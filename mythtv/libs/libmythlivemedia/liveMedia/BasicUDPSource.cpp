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
// A simple UDP source, where every UDP payload is a complete frame
// Implementation

#include "BasicUDPSource.hh"
#include <GroupsockHelper.hh>

BasicUDPSource* BasicUDPSource::createNew(UsageEnvironment& env,
					Groupsock* inputGS) {
  return new BasicUDPSource(env, inputGS);
}

BasicUDPSource::BasicUDPSource(UsageEnvironment& env, Groupsock* inputGS)
  : FramedSource(env), fInputGS(inputGS) {
  // Try to use a large receive buffer (in the OS):
  increaseReceiveBufferTo(env, inputGS->socketNum(), 50*1024);
}

BasicUDPSource::~BasicUDPSource(){
  envir().taskScheduler().turnOffBackgroundReadHandling(fInputGS->socketNum());
}

void BasicUDPSource::doGetNextFrame() {
  // Await the next incoming packet:
  envir().taskScheduler().turnOnBackgroundReadHandling(fInputGS->socketNum(), 
       (TaskScheduler::BackgroundHandlerProc*)&incomingPacketHandler, this);
}

void BasicUDPSource::doStopGettingFrames() {
  envir().taskScheduler().turnOffBackgroundReadHandling(fInputGS->socketNum());
}


void BasicUDPSource::incomingPacketHandler(BasicUDPSource* source, int /*mask*/){
  source->incomingPacketHandler1();
}

void BasicUDPSource::incomingPacketHandler1() {
  if (!isCurrentlyAwaitingData()) return; // we're not ready for the data yet

  // Read the packet into our desired destination:
  struct sockaddr_in fromAddress;
  if (!fInputGS->handleRead(fTo, fMaxSize, fFrameSize, fromAddress)) return;
    
  // Tell our client that we have new data:
  afterGetting(this); // we're preceded by a net read; no infinite recursion
}
