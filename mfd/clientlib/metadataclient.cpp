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
    //
    //  until I'm logged in, I have no session_id
    //
    
    session_id = 0;
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
    //  Another sanity check on the version number of the protocol
    //

    QString protocol_version_string = mdcap_response->getHeader("MDCAP-Server");
    if(protocol_version_string.length() > 0)
    {
        QString substring = protocol_version_string.section("/", 1, 1);
        substring = substring.section(" ", 0, 0);
        bool ok;
        int protocol_major = substring.section(".", 0, 0).toInt(&ok);
        if(ok)
        {
            int protocol_minor = substring.section(".", 1, 1).toInt(&ok);
            if(ok)
            {
                if(
                    protocol_major == MDCAP_PROTOCOL_VERSION_MAJOR &&
                    protocol_minor == MDCAP_PROTOCOL_VERSION_MINOR
                  )
                {
                    //
                    //  Cool
                    //
                }
                else
                {
                    cerr << "metadataclient.o: mdcap protocol mismatch" 
                         << endl;
                    return;
                }          
            }
            else
            {
                cerr << "metadataclient.o: can't parse protocol version from "
                     << "headers" << endl;
            }
        }
        else
        {
            cerr << "metadataclient.o: can't parse protocol version from "
                 << "headers" << endl;
        }
    }
    else
    {
        cerr << "metadataclient.o: mdcap server has no MDCAP-Server header, "
             << "can't determine protocol version"
             << endl;
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
    else if(first_tag == MarkupCodes::login_group)
    {
        parseLogin(mdcap_input);
    }
    else if(first_tag == MarkupCodes::update_group)
    {
        parseUpdate(mdcap_input);
    }
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
    
    
    QString new_service_name;
    int     new_response_status;
    int     new_protocol_major;
    int     new_protocol_minor;
    
    bool all_is_well = true;
    while(rebuilt_internals.size() > 0 && all_is_well)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        
        //
        //  Depending on what the code is, we set various things
        //
        
        switch(content_code)
        {
            case MarkupCodes::name:
                new_service_name = rebuilt_internals.popName();
                break;
                
            case MarkupCodes::status_code:
                new_response_status = rebuilt_internals.popStatus();
                break;
                
            case MarkupCodes::protocol_version:
                rebuilt_internals.popProtocol(
                                                &new_protocol_major, 
                                                &new_protocol_minor
                                             );
                break;
                
            default:
                cerr << "metadataclient.o getting content codes I don't "
                     << "understand while doing parseServerInfo(). "
                     << endl;
                all_is_well = false;

            break;
        }
    }

    if(all_is_well)
    {
        if(
            new_protocol_major == MDCAP_PROTOCOL_VERSION_MAJOR &&
            new_protocol_minor == MDCAP_PROTOCOL_VERSION_MINOR
          )
        {
            if(new_response_status == 200)  // HTTP-like ok
            {
                setName(new_service_name);
                
                //
                //  Ok, the /server-info call worked ... so we should try and
                //  log in.
                //

                MdcapRequest login_request("/login", ip_address);   
                login_request.send(client_socket_to_service);
            }
            else
            {
                cerr << "metadataclient.o: server send back bad status"
                     << endl;
            }
        }
        else
        {
            cerr << "metadataclient.o: wrong mdcap protocol from server" 
                 << endl;
        }
    }
    
    delete group_contents;
}

void MetadataClient::parseLogin(MdcapInput &mdcap_input)
{
    QValueVector<char> *group_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(group_contents);
    
    if(group_code != MarkupCodes::login_group)
    {
        cerr << "metadataclient.o: asked to parseLogin(), but "
             << "group code was not login_group "
             << endl;
        delete group_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(group_contents);
    
    
    int         new_response_status;
    uint32_t    new_session_id;
    
    bool all_is_well = true;
    while(rebuilt_internals.size() > 0 && all_is_well)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        
        //
        //  Depending on what the code is, we set various things
        //
        
        switch(content_code)
        {
            case MarkupCodes::status_code:
                new_response_status = rebuilt_internals.popStatus();
                break;
                
            case MarkupCodes::session_id:
                new_session_id = rebuilt_internals.popSessionId();
                break;
                
            default:
                cerr << "metadataclient.o getting content codes I don't "
                     << "understand while doing parseLogin(). "
                     << endl;
                all_is_well = false;

            break;
        }
    }

    if(all_is_well)
    {
        if(new_response_status == 200 && new_session_id > 0)  // HTTP-like ok
        {
            //
            //  Ok, I can set my session id for all future communications
            //  with this server
            //

            session_id = new_session_id;

            //
            //  Execellent, we have a session number. Time for our first update
            //            
            
            MdcapRequest update_request("/update", ip_address);   
            update_request.addGetVariable("session-id", session_id);
            update_request.send(client_socket_to_service);
            

        }
        else
        {
            cerr << "metadataclient.o: got bad server or bad session id "
                 << "status while doing parseLogin()"
                 << endl;
        }
    }
    
    delete group_contents;
}

void MetadataClient::parseUpdate(MdcapInput &mdcap_input)
{
    //
    //  /update request returns information about how many containers there
    //  are, then what each container's id, name, and current generation is
    //

    QValueVector<char> *group_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(group_contents);
    
    if(group_code != MarkupCodes::update_group)
    {
        cerr << "metadataclient.o: asked to parseUpdate(), but "
             << "group code was not update_group "
             << endl;
        delete group_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(group_contents);

    int collection_count = rebuilt_internals.popCollectionCount();


    //
    //  Make a list of all our current collections ('cause we're going to go
    //  through the whole list from the server in a sec, and if we have
    //  something here at the client end that does not appear in that list,
    //  we'll need to delete it.
    //

    // FIX


    //
    //  For each, collection, parse out the details
    //


    for(int i = 0; i < collection_count; i++)
    {
        QValueVector<char> *collection_contents = new QValueVector<char>;
        char collection_code = rebuilt_internals.popGroup(collection_contents);
        if(collection_code != MarkupCodes::collection_group)
        {
            cerr << "metadataclient.o: trying to parse a collection group "
                 << "inside an update group, but not getting collection_"
                 << "group as the content code. Buggered!"
                 << endl;

            delete collection_contents;
            return;   
        }
        
        //int collection_id = parseCollection(collection_contents)
    }

    delete group_contents;
}

MetadataClient::~MetadataClient()
{
}




