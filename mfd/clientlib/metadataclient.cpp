/*
	metadataclient.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's metadata service

*/

#include <iostream>
using namespace std;

#include "metadataclient.h"
#include "mfdinterface.h"

#include "mdcaprequest.h"
#include "mdcapresponse.h"

#include "../mdcaplib/markupcodes.h"

MetadataClient::MetadataClient(
                            MfdInterface *the_mfd,
                            int an_mfd,
                            const QString &l_ip_address,
                            uint l_port
                        )
            :ServiceClient(
                            the_mfd,
                            an_mfd,
                            MFD_SERVICE_METADATA,
                            l_ip_address,
                            l_port
                          )
{
}

void MetadataClient::sendFirstRequest()
{
    //
    //  We get the client-server communications to the metadata server
    //  started by making a mdcap /server-info request
    //

    MdcapRequest first_request("/server-info", ip_address);   
    first_request.send(client_socket_to_service); 
}

void MetadataClient::handleIncoming()
{
    //
    //  Collect the incoming data. We need to sit here until we get exactly
    //  as many bytes as the server said we should. Note that this assumes
    //  that:
    //
    //      ! the incoming http (i.e. daap) stuff will *always* include a
    //        *correct* Content-Length header
    //
    //      ! that all the headers and the blank line separating headers
    //        from payload will arrive in the first block!!!
    //
    //      ! that the buffer_size is big enough to get through all the
    //        headers
    //  
    //  Obviously, this code could be made far more robust.
    //
    
    static int buffer_size = 8192;
    char incoming[buffer_size];
    int length = 0;

    MdcapResponse *new_response = NULL;
    
    length = client_socket_to_service->readBlock(incoming, buffer_size);
    
    while(length > 0)
    {
        if(!new_response)
        {
            new_response = new MdcapResponse(incoming, length);
        }
        else
        {
            new_response->appendToPayload(incoming, length);
        }

        if(new_response->complete())
        {
            length = 0;
        }
        else
        {
            if(new_response->allIsWell())
            {
                bool wait_a_while = true;
                while(wait_a_while)
                {
                    bool server_still_there = true;
                    
                    if(client_socket_to_service->waitForMore(1000, &server_still_there) < 1)
                    {
                        if(!server_still_there)
                        {
                            new_response->allIsWell(false);
                            wait_a_while = false;
                            length = 0;
                        }
                    }
                    else
                    {
                        length = client_socket_to_service->readBlock(incoming, buffer_size);
                        if(length > 0)
                        {
                            wait_a_while = false;
                        }
                    }
                }
            }
            else
            {
                length = 0;
            }
        }
    }
    
    if(new_response)
    {
        if(new_response->allIsWell())
        {
            processResponse(new_response);
            delete new_response;
            new_response = NULL;
        }
        else
        {
            //
            //  Well, something is borked.
            //
            
            cerr << "there is something seriously wrong with this metdata server" << endl;
            
        }
    }
}

void MetadataClient::processResponse(MdcapResponse *mdcap_response)
{

    //
    //  First a sanity check to make sure we are getting x-mdcap tagged
    //  responses
    //

    if(
        mdcap_response->getHeader("Content-Type") != 
        "application/x-mdcap-tagged"
      )
    {
        cerr << "metadataclient.o: getting responses from metadata server "
             << "that are not x-mdcap-tagged, which is pretty much a disaster."
             << " Giving up."
             << endl;
        return;
    }
    

    //
    //  Create an mdcap input object with the payload from the response
    //
    
    MdcapInput mdcap_input(mdcap_response->getPayload());
    
    char first_tag = mdcap_input.peekAtNextCode();
    
    if(first_tag == MarkupCodes::server_info_group)
    {
        parseServerInfo(mdcap_input);
    }
    //else if
    //{
    //}
    else
    {
        cerr << "metadataclient.o: did not understand first markup code "
             << "in a mdcap payload "
             << endl;
    }
}

void MetadataClient::parseServerInfo(MdcapInput &mdcap_input)
{
    QValueVector<char> *group_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(group_contents);
    
    if(group_code != MarkupCodes::server_info_group)
    {
        cerr << "metadataclient.o: asked to parseServerInfo(), but "
             << "group code was not server_info_group "
             << endl;
        delete group_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(group_contents);
    
    while(rebuilt_internals.size() > 0)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        
        //
        //  Depending on what the code is, we set various things
        //
        
        if(content_code == MarkupCodes::name)
        {
            QString service_name = rebuilt_internals.popName();
            cout << "I see the service name as " << service_name << endl;
        }
        else if(content_code == MarkupCodes::status_code)
        {
            int response_status = rebuilt_internals.popStatus();
            cout << "I see the server's status as " << response_status << endl;
        }
        else if(content_code == MarkupCodes::protocol_version)
        {
            int protocol_minor = 0;
            int protocol_major = 0;
            rebuilt_internals.popProtocol(&protocol_major, &protocol_minor);
            cout << "I see the protcol version as " << protocol_major << "." << protocol_minor << endl;
        }
        else
        {
            cerr << "metadataclient.o getting content codes I don't understand "
                 << "while doing parseServerInfo(). Code I got was "
                 << (int) content_code
                 << endl;
            break;
        }
    }
    
    delete group_contents;
}

MetadataClient::~MetadataClient()
{
}




