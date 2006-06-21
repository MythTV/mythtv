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
// RTP sink for MPEG-4 Elementary Stream video (RFC 3016)
// C++ header

#ifndef _MPEG4ES_VIDEO_RTP_SINK_HH
#define _MPEG4ES_VIDEO_RTP_SINK_HH

#ifndef _VIDEO_RTP_SINK_HH
#include "VideoRTPSink.hh"
#endif

class MPEG4ESVideoRTPSink: public VideoRTPSink {
public:
  static MPEG4ESVideoRTPSink* createNew(UsageEnvironment& env,
					Groupsock* RTPgs,
					unsigned char rtpPayloadFormat,
					u_int32_t rtpTimestampFrequency = 90000);

protected:
  MPEG4ESVideoRTPSink(UsageEnvironment& env, Groupsock* RTPgs,
		      unsigned char rtpPayloadFormat,
		      u_int32_t rtpTimestampFrequency);
	// called only by createNew()

  virtual ~MPEG4ESVideoRTPSink();

private: // redefined virtual functions:
  virtual Boolean sourceIsCompatibleWithUs(MediaSource& source);

  virtual void doSpecialFrameHandling(unsigned fragmentationOffset,
                                      unsigned char* frameStart,
                                      unsigned numBytesInFrame,
                                      struct timeval frameTimestamp,
                                      unsigned numRemainingBytes);
  virtual Boolean allowFragmentationAfterStart() const;
  virtual Boolean
  frameCanAppearAfterPacketStart(unsigned char const* frameStart,
				 unsigned numBytesInFrame) const;

  virtual char const* auxSDPLine();

private:
  Boolean fVOPIsPresent;
  char* fAuxSDPLine;
};

#endif
