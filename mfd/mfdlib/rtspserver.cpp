/*
	rtspserver.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An rtsp server (as an mfd plugin). 90% + functionality comes from the
	liveMedia RTSP/RTCP/RTP library. This class just really gives it a
	thread to run in.

*/

#include "../config.h"
#ifdef MFD_RTSP_SUPPORT



#include <iostream>
using namespace std;

#include "rtspserver.h"

MFDRtspPlugin::MFDRtspPlugin(
                                MFD *owner, 
                                int identifier, 
                                uint port, 
                                const QString &a_name
                            )
                 :MFDServicePlugin(
                                    owner, 
                                    identifier, 
                                    0, 
                                    a_name, 
                                    false,
                                    0
                                  )
{
    actual_rtsp_port = port;
}

void MFDRtspPlugin::run()
{
    //
    //  This is blindingly complicated
    //
    
    while(keep_going)
    {
        sleep(2);
    }
}

MFDRtspPlugin::~MFDRtspPlugin()
{
}


#endif
