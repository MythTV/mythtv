/*
	rtspserver.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An rtsp server (as an mfd plugin).

*/

#include <iostream>
using namespace std;

#include <qobject.h>
#include <qregexp.h>

#include "rtspserver.h"
#include "rtspinrequest.h"
#include "rtspoutresponse.h"

#define INCOMING_BUFFER_SIZE    49152   // Essentially arbitrary

MFDRtspPlugin::MFDRtspPlugin(
                                MFD *owner, 
                                int identifier, 
                                uint port, 
                                const QString &a_name,
                                int l_minimum_thread_pool_size
                            )
                 :MFDServicePlugin(
                                    owner, 
                                    identifier, 
                                    port, 
                                    a_name, 
                                    true, // use thread pool
                                    l_minimum_thread_pool_size
                                  )
{
}

void MFDRtspPlugin::processRequest(MFDServiceClientSocket *a_client)
{


    if(!a_client)
    {
        warning("%1 (rtsp) tried to run a processRequest() on a client that does not exist");
        return;
    }

    //
    //  We create an RtspInRequest object and let it parse in the bytes
    //  coming over the socket
    //

    int client_id = a_client->getIdentifier();
    a_client->lockReadMutex();
        RtspInRequest *in_request = new RtspInRequest(this, a_client);
    bool status = in_request->parseIncomingBytes();
    a_client->unlockReadMutex();
    a_client->setReading(false);

    if (status)
    {
        //
        //  Call the relevant method depending on what kind of request it is
        //
        
        switch (in_request->getRequest())
        {
            case    RRT_DESCRIBE:
                    handleDescribeRequest(in_request, client_id);
                    break;
                    
            case    RRT_GET_PARAMETER:
                    handleGetParameteRequest(in_request, client_id);
                    break;
                    
            case    RRT_OPTIONS:
                    handleOptionsRequest(in_request, client_id);
                    break;
                    
            case    RRT_PAUSE:
                    handlePauseRequest(in_request, client_id);
                    break;
                    
            case    RRT_PLAY:
                    handlePlayRequest(in_request, client_id);
                    break;

            case    RRT_PING:
                    handlePingRequest(in_request, client_id);
                    break;
                    
            case    RRT_REDIRECT:
                    handleRedirectRequest(in_request, client_id);
                    break;
                    
            case    RRT_SETUP:
                    handleSetupRequest(in_request, client_id);
                    break;
                    
            case    RRT_SET_PARAMETER:
                    handleSetParameterRequest(in_request, client_id);
                    break;

            case    RRT_TEARDOWN:
                    handleTeardownRequest(in_request, client_id);
                    break;

            default:
                    warning("unkown request type coming in");
                    break;        
        }
        
    }
    
    delete in_request;
}

void MFDRtspPlugin::sendResponse(int client_id, RtspOutResponse *rtsp_response)
{
    MFDServiceClientSocket *which_client = NULL;

    client_sockets_mutex.lock();
        QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
        MFDServiceClientSocket *a_client;
        while ( (a_client = iterator.current()) != 0 )
        {
            ++iterator;
            if(a_client->getIdentifier() == client_id)
            {
                which_client = a_client;
                break;
            }
        }
    client_sockets_mutex.unlock();

    if(!which_client)
    {
        log("wanted to send an rtsp response, but the client socket in question went away", 7);
        return;
    }

    which_client->lockWriteMutex();
        
        //
        //  Assemble and send the message
        //

        rtsp_response->send(a_client);

    which_client->unlockWriteMutex();
}



void MFDRtspPlugin::handleDescribeRequest(RtspInRequest* /* in_request */, int /* client_id */)
{
}

void MFDRtspPlugin::handleGetParameteRequest(RtspInRequest* /* in_request */, int /* client_id */)
{
}

void MFDRtspPlugin::handleOptionsRequest(RtspInRequest *in_request, int client_id)
{
    RtspOutResponse *rtsp_response = new RtspOutResponse(this, in_request->getCSeq());
    rtsp_response->addHeader("Public", "OPTIONS, SETUP, TEARDOWN, PLAY");
    sendResponse(client_id, rtsp_response);
    
    delete rtsp_response;

}

void MFDRtspPlugin::handlePauseRequest(RtspInRequest* /* in_request */, int /* client_id */)
{
}

void MFDRtspPlugin::handlePlayRequest(RtspInRequest* /* in_request */, int /* client_id */)
{
}

void MFDRtspPlugin::handlePingRequest(RtspInRequest* /* in_request */, int /* client_id */)
{
}

void MFDRtspPlugin::handleRedirectRequest(RtspInRequest* /* in_request */, int /* client_id */)
{
}

void MFDRtspPlugin::handleSetupRequest(RtspInRequest* /* in_request */, int /* client_id */)
{
}

void MFDRtspPlugin::handleSetParameterRequest(RtspInRequest* /* in_request */, int /* client_id */)
{
}

void MFDRtspPlugin::handleTeardownRequest(RtspInRequest* /* in_request */, int /* client_id */)
{
}




MFDRtspPlugin::~MFDRtspPlugin()
{
}


