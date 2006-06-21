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
// Select from multiple, prioritized RTP streams, based on RTP sequence
// number, producing a single output stream
// Implementation

#include "PrioritizedRTPStreamSelector.hh"
#include "GroupsockHelper.hh"
#include <string.h>
#include <stdlib.h>

////////// PrioritizedInputStreamDescriptor //////////

// A data structure used to describe each input stream:
class PrioritizedInputStreamDescriptor {
public:
  PrioritizedInputStreamDescriptor(PrioritizedRTPStreamSelector*
				   ourSelector,
				   PrioritizedInputStreamDescriptor* next,
				   unsigned priority,
				   RTPSource* inputStream,
				   RTCPInstance* inputStreamRTCP);
  virtual ~PrioritizedInputStreamDescriptor();

  PrioritizedInputStreamDescriptor*& next() { return fNext; }
  unsigned priority() const { return fPriority; }
  RTPSource* rtpStream() const { return fRTPStream; }
  unsigned char*& buffer() { return fBuffer; }
  unsigned bufferSize() const { return fBufferSize; }

  void afterGettingFrame1(unsigned frameSize);
  void onSourceClosure1();

private:
  PrioritizedRTPStreamSelector* fOurSelector;
  PrioritizedInputStreamDescriptor* fNext;
  unsigned fPriority;
  RTPSource* fRTPStream;
  RTCPInstance* fRTCPStream;
  unsigned char* fBuffer; // where to put the next frame from this stream
  static unsigned const fBufferSize;
  unsigned fBufferBytesUsed;
};

static void afterGettingFrame(void* clientData, unsigned frameSize,
			      unsigned numTruncatedBytes,
			      struct timeval presentationTime,
			      unsigned durationInMicroseconds);
static void onSourceClosure(void* clientData);


////////// PacketWarehouse //////////

class WarehousedPacketDescriptor; // forward

class PacketWarehouse {
public:
  PacketWarehouse(unsigned seqNumStagger);
  virtual ~PacketWarehouse();

  Boolean isFull();
  void addNewFrame(unsigned priority, unsigned short rtpSeqNo,
		   unsigned char* buffer, unsigned frameSize);
  unsigned char* dequeueFrame(unsigned& resultFrameSize,
			      unsigned& uSecondsToDefer);

  Boolean fLastActionWasIncoming;
private:
  WarehousedPacketDescriptor* fPacketDescriptors;
  Boolean fHaveReceivedFrames;
  unsigned short fMinSeqNumStored, fMaxSeqNumStored;
  unsigned const fMinSpanForDelivery, fMaxSpanForDelivery, fNumDescriptors;
  struct timeval fLastArrivalTime; unsigned short fLastRTPSeqNo;
  unsigned fInterArrivalAveGap; // in microseconds
};

////////// PrioritizedRTPStreamSelector implementation //////////

PrioritizedRTPStreamSelector
::PrioritizedRTPStreamSelector(UsageEnvironment& env,
			       unsigned seqNumStagger)
  : FramedSource(env),
    fNextInputStreamPriority(0), fInputStreams(NULL),
    fAmCurrentlyReading(False), fNeedAFrame(False) {
  fWarehouse = new PacketWarehouse(seqNumStagger);
}

PrioritizedRTPStreamSelector::~PrioritizedRTPStreamSelector() {
  delete fWarehouse;

  while (fInputStreams != NULL) {
    PrioritizedInputStreamDescriptor* inputStream
      = fInputStreams;
    fInputStreams = inputStream->next();
    delete inputStream;
  }
}

PrioritizedRTPStreamSelector* PrioritizedRTPStreamSelector
::createNew(UsageEnvironment& env, unsigned seqNumStagger) {
  return new PrioritizedRTPStreamSelector(env, seqNumStagger);
}

unsigned PrioritizedRTPStreamSelector
::addInputRTPStream(RTPSource* inputStream,
		    RTCPInstance* inputStreamRTCP) {
  fInputStreams
    = new PrioritizedInputStreamDescriptor(this, fInputStreams,
					   fNextInputStreamPriority,
					   inputStream, inputStreamRTCP);
  return fNextInputStreamPriority++;
}

void PrioritizedRTPStreamSelector::removeInputRTPStream(unsigned priority) {
  for (PrioritizedInputStreamDescriptor*& inputStream
	 = fInputStreams;
       inputStream != NULL; inputStream = inputStream->next()) {
    if (inputStream->priority() == priority) {
      PrioritizedInputStreamDescriptor* toDelete
	= inputStream;
      inputStream->next() = toDelete->next();
      delete toDelete;
      break;
    }
  }
}

Boolean PrioritizedRTPStreamSelector
::lookupByName(UsageEnvironment& env,
	       char const* sourceName,
	       PrioritizedRTPStreamSelector*& resultSelector) {
  resultSelector = NULL; // unless we succeed

  FramedSource* source;
  if (!FramedSource::lookupByName(env, sourceName, source)) return False;

  if (!source->isPrioritizedRTPStreamSelector()) {
    env.setResultMsg(sourceName, " is not a Prioritized RTP Stream Selector");
    return False;
  }

  resultSelector = (PrioritizedRTPStreamSelector*)source;
  return True;
}

Boolean PrioritizedRTPStreamSelector
::isPrioritizedRTPStreamSelector() const {
  return True;
}

void PrioritizedRTPStreamSelector::doGetNextFrame() {
  // Begin the process of reading frames from our sources into the
  // frame 'warehouse' (unless this is already ongoing):
  startReadingProcess();

  // (Try to) give ourselves a frame from our warehouse:
  unsigned uSecondsToDefer;
  if (deliverFrameToClient(uSecondsToDefer)) {
    fNeedAFrame = False;

    // Complete the delivery.  If we were told to delay before doing
    // this, then schedule this as a delayed task:
    if (uSecondsToDefer > 0) {
      nextTask()
	= envir().taskScheduler().scheduleDelayedTask((int)uSecondsToDefer,
						      completeDelivery,
						      this);
    } else {
          completeDelivery(this);
    }
  } else {
    fNeedAFrame = True;
  }
}

void PrioritizedRTPStreamSelector::startReadingProcess() {
  if (fAmCurrentlyReading) return; // already ongoing
  if (fWarehouse->isFull()) return; // no room now for any more

  // Run through each input stream, requesting a new frame from it
  // (unless a previous read on this frame is still in progress).
  // When a frame arrives on one of the input streams, it will get
  // stored in our 'warehouse', for later delivery to us.
  for (PrioritizedInputStreamDescriptor* inputStream
	 = fInputStreams;
       inputStream != NULL; inputStream = inputStream->next()) {
    RTPSource* rtpStream = inputStream->rtpStream();
    if (!rtpStream->isCurrentlyAwaitingData()) {
      // Read a frame into this stream's descriptor's buffer:
      fAmCurrentlyReading = True;
      rtpStream->getNextFrame(inputStream->buffer(),
			      inputStream->bufferSize(),
			      afterGettingFrame, inputStream,
			      onSourceClosure, inputStream);
    }
  }
}

void PrioritizedRTPStreamSelector
::handleNewIncomingFrame(unsigned priority, unsigned short rtpSeqNo,
			 unsigned char* buffer, unsigned frameSize) {
  // Begin by adding this new frame to the warehouse:
  fWarehouse->addNewFrame(priority, rtpSeqNo, buffer, frameSize);
  fWarehouse->fLastActionWasIncoming = True;

  // Try again to deliver a frame for our client (if he still wants one):
  if (fNeedAFrame) {
    doGetNextFrame();
  }

  // Continue the reading process:
  fAmCurrentlyReading = False;
  startReadingProcess();
}

Boolean PrioritizedRTPStreamSelector
::deliverFrameToClient(unsigned& uSecondsToDefer) {
  unsigned char* buffer
    = fWarehouse->dequeueFrame(fFrameSize, uSecondsToDefer);

  if (buffer != NULL) {
    // A frame was available
    if (fFrameSize > fMaxSize) {
      fNumTruncatedBytes = fFrameSize - fMaxSize;
      fFrameSize = fMaxSize;
    }
    memmove(fTo, buffer, fFrameSize);

    delete[] buffer;
    fWarehouse->fLastActionWasIncoming = False;
    return True;
  }

  // No frame was available.
  return False;
}


void PrioritizedRTPStreamSelector::completeDelivery(void* clientData) {
  PrioritizedRTPStreamSelector* selector
    = (PrioritizedRTPStreamSelector*)clientData;

  // Call our own 'after getting' function.  Because we're not a 'leaf'
  // source, we can call this directly, without risking infinite recursion.
  FramedSource::afterGetting(selector);
}


//////// PrioritizedInputStreamDescriptor implementation ////////

unsigned const PrioritizedInputStreamDescriptor::fBufferSize = 4000;

PrioritizedInputStreamDescriptor
::PrioritizedInputStreamDescriptor(PrioritizedRTPStreamSelector*
				   ourSelector,
				   PrioritizedInputStreamDescriptor* next,
				   unsigned priority,
				   RTPSource* inputStream,
				   RTCPInstance* inputStreamRTCP)
    : fOurSelector(ourSelector), fNext(next), fPriority(priority),
      fRTPStream(inputStream), fRTCPStream(inputStreamRTCP) {
  fBuffer = new unsigned char[fBufferSize];
  fBufferBytesUsed = 0;
}

PrioritizedInputStreamDescriptor::~PrioritizedInputStreamDescriptor() {
  delete[] fBuffer;
}

static void afterGettingFrame(void* clientData, unsigned frameSize,
			      unsigned /*numTruncatedBytes*/,
			      struct timeval /*presentationTime*/,
			      unsigned /*durationInMicroseconds*/) {
  PrioritizedInputStreamDescriptor* inputStream
    = (PrioritizedInputStreamDescriptor*)clientData;
  inputStream->afterGettingFrame1(frameSize);
}

void PrioritizedInputStreamDescriptor
::afterGettingFrame1(unsigned frameSize) {
  unsigned short rtpSeqNo = rtpStream()->curPacketRTPSeqNum();
  // Deliver this frame to our selector:
  fOurSelector->handleNewIncomingFrame(fPriority, rtpSeqNo,
				       fBuffer, frameSize);
}

static void onSourceClosure(void* clientData) {
  PrioritizedInputStreamDescriptor* inputStream
    = (PrioritizedInputStreamDescriptor*)clientData;
  inputStream->onSourceClosure1();
}

void PrioritizedInputStreamDescriptor::onSourceClosure1() {
  fOurSelector->removeInputRTPStream(fPriority);
}


////////// WarehousedPacketDescriptor //////////

class WarehousedPacketDescriptor {
public:
  WarehousedPacketDescriptor() : buffer(NULL) {}
  // Don't define a destructor; for some reason it causes a crash #####

  unsigned priority;
  unsigned frameSize;
  unsigned char* buffer;
};


////////// PacketWarehouse implementation 

PacketWarehouse::PacketWarehouse(unsigned seqNumStagger)
  : fLastActionWasIncoming(False),
    fHaveReceivedFrames(False), fMinSeqNumStored(0), fMaxSeqNumStored(0),
    fMinSpanForDelivery((unsigned)(1.5*seqNumStagger)),
    fMaxSpanForDelivery(3*seqNumStagger),
    fNumDescriptors(4*seqNumStagger),
    fInterArrivalAveGap(0) {
  fPacketDescriptors = new WarehousedPacketDescriptor[fNumDescriptors];
  if (fPacketDescriptors == NULL) {
#ifdef DEBUG
    fprintf(stderr, "PacketWarehouse failed to allocate %d descriptors; seqNumStagger too large!\n", fNumDescriptors);
#endif
    exit(1);
  }

  // Initially, set "fLastArrivalTime" to the current time:
  gettimeofday(&fLastArrivalTime, NULL);
}

PacketWarehouse::~PacketWarehouse() {
  // Delete each descriptor's buffer (if any), then delete the descriptors:
  for (unsigned i = 0; i < fNumDescriptors; ++i) {
    delete[] fPacketDescriptors[i].buffer;
  }
  delete[] fPacketDescriptors;
}

Boolean PacketWarehouse::isFull() {
  int currentSpan = fMaxSeqNumStored - fMinSeqNumStored;
  if (currentSpan < 0) currentSpan += 65536;

  return (unsigned)currentSpan >= fNumDescriptors;
}

void PacketWarehouse::addNewFrame(unsigned priority,
				  unsigned short rtpSeqNo,
				  unsigned char* buffer,
				  unsigned frameSize) {
  if (!fHaveReceivedFrames) {
    // This is our first frame; set up initial parameters:
    // (But note hack: We want the first frame to have priority 0, so that
    //  receivers' decoders are happy, by seeing the 'best' data initially)
    if (priority != 0) return;

    fMinSeqNumStored = fMaxSeqNumStored = rtpSeqNo;
    fHaveReceivedFrames = True;
  } else {
    // Update our record of the maximum sequence number stored
    if (seqNumLT(fMaxSeqNumStored, rtpSeqNo)) {
      fMaxSeqNumStored = rtpSeqNo;
    } else if (seqNumLT(rtpSeqNo, fMinSeqNumStored)) {
      return; // ignore this packet; it's too old for us
    }
  }

  if (isFull()) {
    // We've gotten way ahead of ourselves, probably due to a very large
    // set of consecutive lost packets.  To recover, reset our min/max
    // sequence numbers (effectively emptying the warehouse).  Hopefully,
    // this should be an unusual occurrence:
    fMinSeqNumStored = fMaxSeqNumStored = rtpSeqNo;
  }

  // Check whether a frame with this sequence number has already been seen
  WarehousedPacketDescriptor& desc
    = fPacketDescriptors[rtpSeqNo%fNumDescriptors];
  if (desc.buffer != NULL) {
    // We already have a frame.  If it's priority is higher than that of
    // this new frame, then we continue to use it:
    if (desc.priority < priority) return; // lower than existing priority

    // Otherwise, use the new frame instead, so get rid of the existing one:
    delete[] desc.buffer;
  }

  // Record this new frame:
  desc.buffer = new unsigned char[frameSize];
  if (desc.buffer == NULL) {
#ifdef DEBUG
    fprintf(stderr, "PacketWarehouse::addNewFrame failed to allocate %d-byte buffer!\n", frameSize);
#endif
    exit(1);
  }
  memmove(desc.buffer, buffer, frameSize);
  desc.frameSize = frameSize;
  desc.priority = priority;

  struct timeval timeNow;
  gettimeofday(&timeNow, NULL);

  if (rtpSeqNo == (fLastRTPSeqNo+1)%65536) {
    // We've received consecutive packets, so update the estimate of
    // the average inter-arrival gap:
    unsigned lastGap // in microseconds
      = (timeNow.tv_sec - fLastArrivalTime.tv_sec)*1000000
      + (timeNow.tv_usec - fLastArrivalTime.tv_usec);

    fInterArrivalAveGap = (9*fInterArrivalAveGap + lastGap)/10; // weighted
  }
  fLastArrivalTime = timeNow;
  fLastRTPSeqNo = rtpSeqNo;
}

unsigned char* PacketWarehouse::dequeueFrame(unsigned& resultFrameSize,
					     unsigned& uSecondsToDefer) {
  uSecondsToDefer = 0; // by default

  // Don't return anything if we don't yet have enough packets to
  // cover our desired sequence number span
  int currentSpan = fMaxSeqNumStored - fMinSeqNumStored;
  if (currentSpan < 0) currentSpan += 65536;
  if (currentSpan < (int)fMinSpanForDelivery) {
    return NULL; // we're not ready
  }

  // Now, if our stored packet range is less than the desired maximum,
  // return a frame, but delay this unless no incoming frames have
  // arrived since the last time a frame was delivered to our client.
  // This causes the buffer to empty only if the flow of incoming
  // frames stops.
  if (currentSpan < (int)fMaxSpanForDelivery) {
    if (fLastActionWasIncoming) {
      uSecondsToDefer = (unsigned)(1.5*fInterArrivalAveGap);
    }
  }

  // Now, return a frame:
  unsigned char* resultBuffer = NULL;
  do {
    if (currentSpan < (int)fMinSpanForDelivery) break;

    WarehousedPacketDescriptor& desc
      = fPacketDescriptors[fMinSeqNumStored%fNumDescriptors];
    resultBuffer = desc.buffer;
    resultFrameSize = desc.frameSize;

    desc.buffer = NULL;
#ifdef DEBUG
    if (resultBuffer == NULL) fprintf(stderr, "No packet for seq num %d - skipping\n", fMinSeqNumStored); //#####
  else if (desc.priority == 2) fprintf(stderr, "Using priority %d frame for seq num %d\n", desc.priority, fMinSeqNumStored);//#####
#endif
    fMinSeqNumStored = (fMinSeqNumStored+1)%65536;
    --currentSpan;
  } while (resultBuffer == NULL); // skip over missing packets

  return resultBuffer;
}
