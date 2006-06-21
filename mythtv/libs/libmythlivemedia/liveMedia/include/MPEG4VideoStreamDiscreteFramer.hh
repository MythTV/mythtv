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
// A simplified version of "MPEG4VideoStreamFramer" that takes only complete,
// discrete frames (rather than an arbitrary byte stream) as input.
// This avoids the parsing and data copying overhead of the full
// "MPEG4VideoStreamFramer".
// C++ header

#ifndef _MPEG4_VIDEO_STREAM_DISCRETE_FRAMER_HH
#define _MPEG4_VIDEO_STREAM_DISCRETE_FRAMER_HH

#ifndef _MPEG4_VIDEO_STREAM_FRAMER_HH
#include "MPEG4VideoStreamFramer.hh"
#endif

class MPEG4VideoStreamDiscreteFramer: public MPEG4VideoStreamFramer {
public:
  static MPEG4VideoStreamDiscreteFramer*
  createNew(UsageEnvironment& env, FramedSource* inputSource);

private:
  MPEG4VideoStreamDiscreteFramer(UsageEnvironment& env,
				 FramedSource* inputSource);
      // called only by createNew()
  virtual ~MPEG4VideoStreamDiscreteFramer();

private:
  // redefined virtual functions:
  virtual void doGetNextFrame();

private:
  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
                                struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame1(unsigned frameSize,
                          unsigned numTruncatedBytes,
                          struct timeval presentationTime,
                          unsigned durationInMicroseconds);

  Boolean getNextFrameBit(u_int8_t& result);
  Boolean getNextFrameBits(unsigned numBits, u_int32_t& result);
    // Which are used by:
  void analyzeVOLHeader();

private:
  unsigned fNumBitsSeenSoFar; // used by the getNextFrameBit*() routines
  u_int32_t vop_time_increment_resolution;
  unsigned fNumVTIRBits;
  // # of bits needed to count to "vop_time_increment_resolution"
  struct timeval fLastNonBFramePresentationTime;
  unsigned fLastNonBFrameVop_time_increment;
};

#endif
