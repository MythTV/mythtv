#ifndef RTSPOUT_H_
#define RTSPOUT_H_
/*
	rtspout.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an rtsp/rtp server that sends out WAV/PCM data of whatever
	is currently being played by the audio plugin. Intended for client side
	visualization, but could conceivably be used to make clients that tune
	into the "Live" RTP broadcast.
*/

#include "rtspserver.h"

class RtspOut: public MFDRtspPlugin
{

  public:

    RtspOut(MFD *owner, int identity);
    ~RtspOut();

    //
    //  Implement the RTSP requests we can deal with
    //
    
    void handleOptionsRequest(RtspInRequest *in_request, int client_id);
    void handleDescribeRequest(RtspInRequest *in_request, int client_id);

    void run();

  private:
  
    QString my_ip_address;

};

#endif  // rtspout_h_
