/*
	rtspout.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an rtsp/rtp server that sends out WAV/PCM data of whatever
	is currently being played by the audio plugin. Intended for client side
	visualization, but could conceivably be used to make clients that tune
	into the "Live" RTP broadcast.
*/

#include <iostream>
using namespace std;

#include "rtspout.h"

RtspOut::RtspOut(MFD *owner, int identity)
      :MFDRtspPlugin(owner, identity, 2346, "audio rtsp server", 1)
{
    log("sub-plugin came into existence", 2);
}


void RtspOut::run()
{
    char my_hostname[2049];
    QString local_hostname = "unknown";

    if (gethostname(my_hostname, 2048) < 0)
    {
        warning("could not call gethostname()");
    }
    else
    {
        local_hostname = my_hostname;
    }

    //
    //  Have our rtsp service broadcast on the local lan
    //

    Service *rtsp_service = new Service(
                                        QString("Myth Audio RTSP on %1").arg(local_hostname),
                                        QString("rtsp"),
                                        local_hostname,
                                        SLT_HOST,
                                        (uint) port_number
                                       );
 
    ServiceEvent *se = new ServiceEvent( true, true, *rtsp_service);
    QApplication::postEvent(parent, se);
    delete rtsp_service;

    //
    //  Init our sockets
    //
    
    if ( !initServerSocket())
    {
        fatal("rtsp sub-plugin could not initialize its core server socket");
        return;
    }
    
    while(keep_going)
    {
        //
        //  Update the status of our sockets and wait for something to happen
        //
        
        updateSockets();
        waitForSomethingToHappen();
    }
}

RtspOut::~RtspOut()
{
    log("sub-plugin is being snuffed out", 2);
}

