/*
	rtspout.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an rtsp/rtp server that sends out WAV/PCM data of whatever
	is currently being played by the audio plugin. Intended for client side
	visualization, but could conceivably be used to make clients that tune
	into the "Live" RTP broadcast.
*/

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
              
#include <iostream>
using namespace std;

#include "rtspinrequest.h"
#include "rtspoutresponse.h"
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



void RtspOut::handleOptionsRequest(RtspInRequest *in_request, int client_id)
{
    //
    //  Send a response that describes the capabilities of this RTSP server.
    //  Note, only the minimal Public: stuff is supported at this point
    //
    
    RtspOutResponse *rtsp_response = new RtspOutResponse(this, in_request->getCSeq());
    rtsp_response->addHeader("Public", "OPTIONS, SETUP, TEARDOWN, PLAY");
    sendResponse(client_id, rtsp_response);
    
    delete rtsp_response;

}

void RtspOut::handleDescribeRequest(RtspInRequest *in_request, int client_id)
{
    //
    //  See if the client accepts application/sdp ('cause that's the only
    //  way this plugin knows to describe a session)
    //
    
    QString accept_header = in_request->getHeaderValue("Accept");

    if (accept_header != "application/sdp")
    {
        log(QString("client (User-Agent = \"%1\") did not explicitly say that "
                    "it accepts sdp descriptions, but we're going to send one "
                    "anyway").arg(in_request->getHeaderValue("User-Agent")), 3);
    }

    //
    //  So ... we need to create and SDP description of what we are (and
    //  what we are is just a LIVE stream of whatever base audio plugin
    //  happens to playing at the moment.
    //
    
    //
    //  Need local IP address
    //
    
    QString local_hostname = "unknown";
    QString local_ip = "127.0.0.1";
    char my_hostname[2049];
    if(gethostname(my_hostname, 2048) < 0)
    {
        warning("could not call gethostname()");
        return;
    }
    else
    {
        local_hostname = my_hostname;
        struct hostent *gethostbyname(const char *name);
        
    }           


    //  total hack at the moment
    
    RtspOutResponse *rtsp_response = new RtspOutResponse(this, in_request->getCSeq());
    rtsp_response->addHeader("Content-Base", in_request->getUrl());
    rtsp_response->addHeader("Content-Type", "application/sdp");

    rtsp_response->addTextToPayload("v=0"); // start line of sdp response
    rtsp_response->addTextToPayload(QString("o=- 56565636536 1 IN IP4 %1").arg(my_ip_address));
    rtsp_response->addTextToPayload("s=Session streamed from MythTV audio");
    rtsp_response->addTextToPayload("i=somefile.wav");
    rtsp_response->addTextToPayload("t=0 0");
    rtsp_response->addTextToPayload("a=tool:Myth");
    rtsp_response->addTextToPayload("a=type:broadcast");
    rtsp_response->addTextToPayload("a=control:*");
    rtsp_response->addTextToPayload(QString("a=source-filter: incl IN IP4 * %1").arg(my_ip_address));
    rtsp_response->addTextToPayload("a=rtcp:unicast reflection");
    rtsp_response->addTextToPayload("a=range:npt=0-");
    rtsp_response->addTextToPayload("a=x-qt-text-nam:Session streamed from MythTV audio");
    rtsp_response->addTextToPayload("a=x-qt-text-inf:test.wav");
    rtsp_response->addTextToPayload("m=audio 2222 RTP/AVP 10");
    rtsp_response->addTextToPayload("c=IN IP4 232.16.26.7/255");
    rtsp_response->addTextToPayload("a=control:mmusic-current");

    //rtsp_response->printYourself(true);
    sendResponse(client_id, rtsp_response);
    
    delete rtsp_response;
    
    
}



RtspOut::~RtspOut()
{
    log("sub-plugin is being snuffed out", 2);
}

