/*
	httpserver.cpp

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An http server (as an mfd plugin). Multithreaded 'n everything

*/

#include <iostream>
using namespace std;

#include <qobject.h>
#include <qregexp.h>

#include "httpserver.h"
#include "httpoutresponse.h"

//
//  Yes, it is an entire friggin' http server
//

MFDHttpPlugin::MFDHttpPlugin(
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


void MFDHttpPlugin::processRequest(MFDServiceClientSocket *a_client)
{

    if(!a_client)
    {
        warning("%1 (httpd) tried to run a processRequest() on a client that does not exist");
        return;
    }


    int client_id = a_client->getIdentifier();
    a_client->lockReadMutex();
        HttpInRequest *new_request = new HttpInRequest(this, a_client);
    bool status = new_request->parseIncomingBytes();
    a_client->unlockReadMutex();
    a_client->setReading(false);

    if(new_request->allIsWell() && status)
    {
        if(new_request->parseRequestLine())
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
    }
    delete new_request;
}


void MFDHttpPlugin::handleIncoming(HttpInRequest *, int)
{
    warning("(httpd) called base class handleIncoming()");
}

void MFDHttpPlugin::sendResponse(int client_id, HttpOutResponse *http_response)
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
        log("wanted to send an http response, but the client socket in question went away", 7);
        return;
    }
    which_client->lockWriteMutex();
        
        //
        //  Assemble and send the message
        //

        http_response->send(a_client);

    which_client->unlockWriteMutex();
}

MFDHttpPlugin::~MFDHttpPlugin()
{
}

