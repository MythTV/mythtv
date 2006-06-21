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
// C++ header

#ifndef _PRIORITIZED_RTP_STREAM_SELECTOR_HH
#define _PRIORITIZED_RTP_STREAM_SELECTOR_HH

#ifndef _RTCP_HH
#include "RTCP.hh"
#endif

class PrioritizedRTPStreamSelector: public FramedSource {
public:
  static PrioritizedRTPStreamSelector*
  createNew(UsageEnvironment& env, unsigned seqNumStagger);
    // seqNumStagger is the maximum staggering of RTP sequence #s expected

  unsigned addInputRTPStream(RTPSource* inputStream,
			     RTCPInstance* inputStreamRTCP);
      // the input streams are assumed to be added in priority order;
      // highest priority first.  Returns the priority (0 == highest)
  void removeInputRTPStream(unsigned priority); // also closes stream

  static Boolean lookupByName(UsageEnvironment& env,
			      char const* sourceName,
                              PrioritizedRTPStreamSelector*& resultSource);

  friend class PrioritizedInputStreamDescriptor;

private:
  // Redefined virtual functions:
  virtual Boolean isPrioritizedRTPStreamSelector() const;
  void doGetNextFrame();

private:
  PrioritizedRTPStreamSelector(UsageEnvironment& env,
			       unsigned seqNumStagger);
      // called only by createNew()
  virtual ~PrioritizedRTPStreamSelector();

  void startReadingProcess();

  void handleNewIncomingFrame(unsigned priority, unsigned short rtpSeqNo,
			      unsigned char* buffer, unsigned frameSize);

  Boolean deliverFrameToClient(unsigned& uSecondsToDefer);
  static void completeDelivery(void* clientData);

private:
  unsigned fNextInputStreamPriority;
  class PrioritizedInputStreamDescriptor* fInputStreams;

  class PacketWarehouse* fWarehouse;

  Boolean fAmCurrentlyReading;
  Boolean fNeedAFrame;
};

#endif
