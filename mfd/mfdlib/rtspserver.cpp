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
/*

    char incoming[MAX_CLIENT_INCOMING];    // FIX
    int length = 0;
    int client_id = a_client->getIdentifier();

    //
    //  Copy out the whole block out so we can release the lock straight away.
    //


    a_client->lockReadMutex();
        length = a_client->readBlock(incoming, MAX_CLIENT_INCOMING - 1);
    a_client->unlockReadMutex();
    a_client->setReading(false);
    
    
    if(length > 0)
    {
        if(length >= MAX_CLIENT_INCOMING)
        {
            // oh crap
            warning("client http socket is getting too much data");
            length = MAX_CLIENT_INCOMING;
        }

        HttpInRequest *new_request = new HttpInRequest(this, incoming, length);
        if(new_request->allIsWell())
        {
            //
            //  Pass it to handleIncoming() which should be re-implemented
            //  in any actual plugin
            //
            
            handleIncoming(new_request, client_id);
            if(new_request->sendResponse())
            {
                sendResponse(client_id, new_request->getResponse());
            }
        }
        delete new_request;
    }
*/

}




MFDRtspPlugin::~MFDRtspPlugin()
{
}


