#ifndef RTSPSERVER_H_
#define RTSPSERVER_H_

/*
	rtspserver.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An rtsp server (as an mfd plugin). 90% + functionality comes from the
	liveMedia RTSP/RTCP/RTP library. This class just really gives it a
	thread to run in.


*/

#include "../config.h"

#ifdef MFD_RTSP_SUPPORT




#include "mfd_plugin.h"

class MFDRtspPlugin : public MFDServicePlugin
{

  public:
  
    MFDRtspPlugin(
                    MFD *owner, 
                    int identifier, 
                    uint port, 
                    const QString &a_name = "unknown rtsp"
                 );
    virtual ~MFDRtspPlugin();
    
    void run();

  private:
  
    uint    actual_rtsp_port;

};

#endif

#endif

