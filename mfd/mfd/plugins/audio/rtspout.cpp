/*
	rtspout.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an rtsp/rtp server that sends out WAV/PCM data of whatever
	is currently being played by the audio plugin. Intended for client side
	visualization, but could conceivably be used to make clients that tune
	into the "Live" RTP broadcast.
*/

#include "../../../config.h"
#ifdef MFD_RTSP_SUPPORT

#include "rtspout.h"

RtspOut::RtspOut(MFD *owner, int identity)
      :MFDRtspPlugin(owner, identity, 2346, "audio rtsp server")
{
    log("sub-plugin came into existence", 2);
}


/*

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
        //
        //  TEMP HACK FIX
        //

        local_hostname = my_hostname;
        struct hostent* host;
        host = gethostbyname(local_hostname.ascii());
        u_int8_t const** const hAddrPtr = (u_int8_t const**)host->h_addr_list;
        if (hAddrPtr != NULL) 
        {
            // First, count the number of addresses:
            u_int8_t const** hAddrPtr1 = hAddrPtr;

            uint number_of_addresses = 0;
            while (*hAddrPtr1 != NULL) 
            {
                ++number_of_addresses;
                ++hAddrPtr1;
            }

            // Next, set up the list:
            //fAddressArray = new NetAddress*[fNumAddresses];
            //if (fAddressArray == NULL) return;

            for (unsigned i = 0; i < 1; ++i) 
            {
                //fAddressArray[i] = new NetAddress(hAddrPtr[i], host->h_length);
                struct in_addr addr;
                memcpy(&addr, hAddrPtr[i], sizeof(struct in_addr));
                my_ip_address = QString(inet_ntoa(addr));
            }

        }
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

*/

RtspOut::~RtspOut()
{
    log("sub-plugin is being snuffed out", 2);
}

#endif
