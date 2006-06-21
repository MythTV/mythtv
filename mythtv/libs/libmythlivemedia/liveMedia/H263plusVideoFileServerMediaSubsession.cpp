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
// Copyright (c) 1996-2006 Live Networks, Inc.  All rights reserved.
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from a H263 video file.
// Implementation

// Author: Bernhard Feiten. // Based on MPEG4VideoFileServerMediaSubsession

#include "H263plusVideoFileServerMediaSubsession.hh"
#include "H263plusVideoRTPSink.hh"
#include "ByteStreamFileSource.hh"
#include "H263plusVideoStreamFramer.hh" 

////////////////////////////////////////////////////////////////////////////////
H263plusVideoFileServerMediaSubsession*
H263plusVideoFileServerMediaSubsession::createNew(
                                           UsageEnvironment& env,
					                            char const* fileName,
					                            Boolean reuseFirstSource) {
  return new H263plusVideoFileServerMediaSubsession(env, fileName, reuseFirstSource);
}

////////////////////////////////////////////////////////////////////////////////
H263plusVideoFileServerMediaSubsession::H263plusVideoFileServerMediaSubsession(
	                                        UsageEnvironment& env,
                                           char const* fileName, 
                                           Boolean reuseFirstSource)
                 : FileServerMediaSubsession(env, fileName, reuseFirstSource),
                   fDoneFlag(0) 
{
}

////////////////////////////////////////////////////////////////////////////////
H263plusVideoFileServerMediaSubsession::~H263plusVideoFileServerMediaSubsession() 
{
}

////////////////////////////////////////////////////////////////////////////////
#if 0
static void afterPlayingDummy(void* clientData) {
  H263plusVideoFileServerMediaSubsession* subsess
    = (H263plusVideoFileServerMediaSubsession*)clientData;
  // Signal the event loop that we're done:
  subsess->setDoneFlag();
}
#endif

static void checkForAuxSDPLine(void* clientData) {
  H263plusVideoFileServerMediaSubsession* subsess
    = (H263plusVideoFileServerMediaSubsession*)clientData;
  subsess->checkForAuxSDPLine1();
}

void H263plusVideoFileServerMediaSubsession::checkForAuxSDPLine1() {
  if (fDummyRTPSink->auxSDPLine() != NULL) {
    // Signal the event loop that we're done:
    setDoneFlag();
  } else {
    // try again after a brief delay:
    int uSecsToDelay = 100000; // 100 ms
    nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
			      (TaskFunc*)checkForAuxSDPLine, this);
  }
}

char const* H263plusVideoFileServerMediaSubsession::getAuxSDPLine(
													  RTPSink* rtpSink, 
													  FramedSource* inputSource) 
{
  // Note: For MPEG-4 video files, the 'config' information isn't known
  // until we start reading the file.  This means that "rtpSink"s
  // "auxSDPLine()" will be NULL initially, and we need to start reading
  // data from our file until this changes. 
  // Is this true also for H263 ?? >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // asumption: no aux line needed for H263

//  fDummyRTPSink = rtpSink;
    
  // Start reading the file:
//  fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);
    
  // Check whether the sink's 'auxSDPLine()' is ready:
//  checkForAuxSDPLine(this);
    
//  envir().taskScheduler().doEventLoop(&fDoneFlag);
    
//  char const* auxSDPLine = fDummyRTPSink->auxSDPLine();
//  return auxSDPLine;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
FramedSource* H263plusVideoFileServerMediaSubsession::createNewStreamSource(
														unsigned /*clientSessionId*/, 
														unsigned& estBitrate) 
{
  estBitrate = 500; // kbps, estimate ??

  // Create the video source:
   ByteStreamFileSource* fileSource
        = ByteStreamFileSource::createNew(envir(), fFileName);
   if (fileSource == NULL) return NULL;
   fFileSize = fileSource->fileSize();

   // Create a framer for the Video Elementary Stream:
   return H263plusVideoStreamFramer::createNew(envir(), fileSource);
}

///////////////////////////////////////////////////////////////////////////////
RTPSink* H263plusVideoFileServerMediaSubsession::createNewRTPSink(
                                                    Groupsock* rtpGroupsock,
                                                    unsigned char rtpPayloadTypeIfDynamic,
                                                    FramedSource* /*inputSource*/) 
{
   unsigned char payloadFormatCode;

   if (false)    // some more logic is needed if h263 is dynamic PT 
      payloadFormatCode = rtpPayloadTypeIfDynamic;
   else 
      payloadFormatCode = 34;
   
   return H263plusVideoRTPSink::createNew(envir(), rtpGroupsock, payloadFormatCode);
}



