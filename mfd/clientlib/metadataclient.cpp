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
    
    //
    //  if we remove our delete a metadata collection, it should get deleted from memory
    //
    
    metadata_collections.setAutoDelete(true);
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
        return;
    }
    else if(first_tag == MarkupCodes::login_group)
    {
        parseLogin(mdcap_input);
        return;
    }
    else if(first_tag == MarkupCodes::update_group)
    {
        parseUpdate(mdcap_input);
    }
    else if(first_tag == MarkupCodes::item_group)
    {
        parseItems(mdcap_input);
    }
    else if(first_tag == MarkupCodes::list_group)
    {
        parseLists(mdcap_input);
    } 
    else
    {
        cerr << "metadataclient.o: did not understand first markup code "
             << "in a mdcap payload "
             << endl;
    }
    
    //
    //  Ok, if we're down here, we are logged in and know at least about
    //  which containers exist (although we may well not know anything about
    //  what's in them). Query anything we need to find out
    //
    
    QPtrListIterator<MetadataCollection> it( metadata_collections );
    MetadataCollection *a_collection;
    while ( (a_collection = it.current()) != 0 )
    {
        if(!a_collection->itemsUpToDate())
        {
            //
            //  This collection's items are not up to date, we need to ask
            //  for an update
            //
            
            QString a_request = a_collection->getItemsRequest(session_id);
            MdcapRequest items_request(a_request, ip_address);   
            items_request.send(client_socket_to_service);
            return;
        }
        else if(!a_collection->listsUpToDate())
        {
            //
            //  This collection's lists are not up to date, we need to ask
            //  for an update
            //
            
            QString a_request = a_collection->getListsRequest(session_id);
            MdcapRequest lists_request(a_request, ip_address);   
            lists_request.send(client_socket_to_service);
            return;
        }
        ++it;
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
    //  we'll need to delete it.)
    //

    QValueList<int> pre_existing_collections;

    QPtrListIterator<MetadataCollection> it( metadata_collections );
    MetadataCollection *a_collection;
    while ( (a_collection = it.current()) != 0 )
    {
        ++it;
        pre_existing_collections.append(a_collection->getId());
    }

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
                 << "inside an update group, but not getting "
                 << "collection_group as the content code. Buggered!"
                 << endl;

            delete collection_contents;
            return;   
        }
        
        MdcapInput re_rebuilt_internals(collection_contents);
        int collection_id = parseCollection(re_rebuilt_internals);
        pre_existing_collections.remove(collection_id);
    }

    delete group_contents;

    //
    //  If any collections went away completely, deal with with
    //

    QValueList<int>::iterator away_it;
    for( 
            away_it  = pre_existing_collections.begin(); 
            away_it != pre_existing_collections.end(); 
            ++away_it 
       )
    {
        //  do something
    }
}

void MetadataClient::parseItems(MdcapInput &mdcap_input)
{
    QValueVector<char> *group_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(group_contents);
    
    if(group_code != MarkupCodes::item_group)
    {
        cerr << "metadataclient.o: asked to parseItems(), but "
             << "group code was not item_group "
             << endl;
        delete group_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(group_contents);

    //
    //  Go through the top level tags in an item_group collection, and do
    //  whatever need to be done to parse through all the data sent
    //
    
    int     new_collection_id = -1;
    int     new_collection_generation = -1;
    bool    new_update_type = false;
    int     new_total_items = -1;
    int     new_added_items = -1;
    int     new_deleted_items = -1;

    bool all_is_well = true;
    while(rebuilt_internals.size() > 0 && all_is_well)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        
        //
        //  Depending on what the code is, we set various things
        //
        
        switch(content_code)
        {
            case MarkupCodes::collection_id:
                new_collection_id = rebuilt_internals.popCollectionId();
                break;
                
            case MarkupCodes::collection_generation:
                new_collection_generation = rebuilt_internals.popCollectionGeneration();
                break;
                
            case MarkupCodes::update_type:
                new_update_type = rebuilt_internals.popUpdateType();
                break;
                
            case MarkupCodes::total_items:
                new_total_items = rebuilt_internals.popTotalItems();
                break;
                
            case MarkupCodes::added_items:
                new_added_items = rebuilt_internals.popAddedItems();
                break;
                
            case MarkupCodes::deleted_items:
                new_deleted_items = rebuilt_internals.popDeletedItems();
                break;
                
            case MarkupCodes::added_items_group:
                parseNewItemData(
                                    new_collection_id, 
                                    new_collection_generation,
                                    new_update_type,
                                    new_total_items,
                                    new_added_items,
                                    new_deleted_items,
                                    rebuilt_internals
                                 );
                break;

            default:

                cerr << "metadataclient.o getting content codes I don't "
                     << "understand while doing parseItems(). "
                     << "code is " << (int) content_code
                     << endl;
                all_is_well = false;

            break;
        }
    }
    
}

void MetadataClient::parseLists(MdcapInput &mdcap_input)
{
    QValueVector<char> *group_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(group_contents);
    
    if(group_code != MarkupCodes::list_group)
    {
        cerr << "metadataclient.o: asked to parseLists(), but "
             << "group code was not list_group "
             << endl;
        delete group_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(group_contents);

    //
    //  Go through the top level tags in an item_group collection, and do
    //  whatever need to be done to parse through all the data sent
    //
    
    int     new_collection_id = -1;
    int     new_collection_generation = -1;
    bool    new_update_type = false;
    int     new_total_items = -1;
    int     new_added_items = -1;
    int     new_deleted_items = -1;

    bool all_is_well = true;

    while(rebuilt_internals.size() > 0 && all_is_well)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        
        //
        //  Depending on what the code is, we set various things
        //
        
        switch(content_code)
        {
            case MarkupCodes::collection_id:
                new_collection_id = rebuilt_internals.popCollectionId();
                break;
                
            case MarkupCodes::collection_generation:
                new_collection_generation = rebuilt_internals.popCollectionGeneration();
                break;
                
            case MarkupCodes::update_type:
                new_update_type = rebuilt_internals.popUpdateType();
                break;
                
            case MarkupCodes::total_items:
                new_total_items = rebuilt_internals.popTotalItems();
                break;
                
            case MarkupCodes::added_items:
                new_added_items = rebuilt_internals.popAddedItems();
                break;
                
            case MarkupCodes::deleted_items:
                new_deleted_items = rebuilt_internals.popDeletedItems();
                break;
                
            case MarkupCodes::added_lists_group:
                parseNewListData(
                                    new_collection_id, 
                                    new_collection_generation,
                                    new_update_type,
                                    new_total_items,
                                    new_added_items,
                                    new_deleted_items,
                                    rebuilt_internals
                                 );
                break;

            default:

                cerr << "metadataclient.o getting content codes I don't "
                     << "understand while doing parseLists(). "
                     << "code is " << (int) content_code
                     << endl;
                all_is_well = false;

            break;
        }
    }
    
}

int MetadataClient::parseCollection(MdcapInput &mdcap_input)
{
    //
    //  Look through the guts of a collection group and pull out the id, the
    //  number of items, and the current generation number
    //

    uint32_t new_collection_count = 0;
    uint32_t new_collection_id = 0;
    uint32_t new_collection_generation = 0;
    QString  new_collection_name = "";
    uint32_t new_collection_type = 0;

    bool all_is_well = true;
    while(mdcap_input.size() > 0 && all_is_well)
    {
        char content_code = mdcap_input.peekAtNextCode();
        
        //
        //  Depending on what the code is, we set various things
        //
        
        switch(content_code)
        {
            case MarkupCodes::collection_id:
                new_collection_id =  mdcap_input.popCollectionId();   
                break;
                
            case MarkupCodes::collection_type:
                new_collection_type =  mdcap_input.popCollectionType();   
                break;
                
            case MarkupCodes::collection_count:
                new_collection_count = mdcap_input.popCollectionCount();
                break;
                
            case MarkupCodes::name:
                new_collection_name = mdcap_input.popName();
                break;
                
            case MarkupCodes::collection_generation:
                new_collection_generation = mdcap_input.popCollectionGeneration();
                break;
                
            default:
                cerr << "metadataclient.o getting content codes I don't "
                     << "understand ("
                     << (int) content_code
                     << ") while doing parseCollection(). "
                     << endl;
                all_is_well = false;

            break;
        }
    }
    
    if(all_is_well && new_collection_id > 0)
    {
        //
        //  See if we have already seen this collection
        //
        
        QPtrListIterator<MetadataCollection> it( metadata_collections );
        MetadataCollection *a_collection = NULL;
        
        for( ; it.current(); ++it)
        {
            if(it.current()->getId() == (int) new_collection_id)
            {
                a_collection = it.current();
                break;
            }
        }

        if(a_collection)
        {
            //
            //  update an existing collection
            //
            
        }
        else
        {
            //
            //  create a new collection
            //
            
            MetadataType metadata_type = MDT_unknown;
            if(new_collection_type == MDT_audio)
            {
                metadata_type = MDT_audio;
            }
            else if(new_collection_type == MDT_video)
            {
                metadata_type = MDT_video;
            }
            cout << "creating a new collection with id of " << new_collection_id << endl;
            MetadataCollection *new_collection = 
                new MetadataCollection(
                                        new_collection_id,
                                        new_collection_count,
                                        new_collection_name,
                                        new_collection_generation,
                                        metadata_type
                                       );
            metadata_collections.append(new_collection);
        }
             
        return new_collection_id;
    }
    else
    {
        cerr << "problem with a collection" << endl;
    }
    
    return -1;
}

void MetadataClient::parseNewItemData(
                                        int  collection_id,
                                        int  collection_generation,
                                        bool update_type,
                                        int  total_count,
                                        int  add_count,
                                        int  del_count,
                                        MdcapInput &mdcap_input
                                     )
{
    //
    //  Make sure all the incoming stuff is set
    //
    
    if(
        collection_id < 1           ||
        collection_generation < 1   ||
        total_count < 0             ||
        add_count < 0               ||
        del_count < 0
      )
    {
        cerr << "metadataclient.o: asked to parseNewItemData(), but passed "
             << "invalid parameter(s)"
             << endl;
        return;
    }
    

    //
    //  Make sure we are dealing with what we think we're dealing with
    //

    QValueVector<char> *new_item_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(new_item_contents);

    if(group_code != MarkupCodes::added_items_group)
    {
        cerr << "metadataclient.o: asked to parseNewItemData(), but "
             << "group code was not addded_items_group, it was "
             << (int) group_code
             << endl;
        delete new_item_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(new_item_contents);

    //
    //  Try and find the right collection
    //

    QPtrListIterator<MetadataCollection> it( metadata_collections );
    MetadataCollection *which_one = NULL;
    MetadataCollection *a_collection;
    while ( (a_collection = it.current()) != 0 )
    {
        ++it;
        if(a_collection->getId() == collection_id)
        {
            which_one = a_collection;
            break;
        }
    }
    
    if(!which_one)
    {
        cerr << "metadataclient.o: asked to parseNewItemData(), but "
             << "passed collection id is invalid"
             << endl;
             delete new_item_contents;
             return;
    }

    

    if(update_type)
    {
        //
        //  It's all the items (not a delta)
        //

        which_one->clearAllMetadata();
    }

    while(rebuilt_internals.size() > 0)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        if(content_code == MarkupCodes::added_item_group)
        {
            QValueVector<char> *item_contents = new QValueVector<char>;
            rebuilt_internals.popGroup(item_contents);
            MdcapInput re_rebuilt_internals(item_contents);
            which_one->addItem(re_rebuilt_internals);
            delete item_contents;
        }
        else
        {
            cerr << "metadataclient.o: while doing parseNewItemData(), "
                 << "seeing content codes that should not be there"
                 << endl;
            break;
        }
    }

    //
    //  Mark this collections metadata with the right generation number
    //

    which_one->setMetadataGeneration(collection_generation);

    delete new_item_contents;
}
                                        
void MetadataClient::parseNewListData(
                                        int  collection_id,
                                        int  collection_generation,
                                        bool update_type,
                                        int  total_count,
                                        int  add_count,
                                        int  del_count,
                                        MdcapInput &mdcap_input
                                     )
{
    //
    //  Make sure all the incoming stuff is set
    //
    
    if(
        collection_id < 1           ||
        collection_generation < 1   ||
        total_count < 0             ||
        add_count < 0               ||
        del_count < 0
      )
    {
        cerr << "metadataclient.o: asked to parseNewListData(), but passed "
             << "invalid parameter(s)"
             << endl;
        return;
    }
    

    //
    //  Make sure we are dealing with what we think we're dealing with
    //

    QValueVector<char> *new_list_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(new_list_contents);

    if(group_code != MarkupCodes::added_lists_group)
    {
        cerr << "metadataclient.o: asked to parseNewListData(), but "
             << "group code was not addded_listss_group, it was "
             << (int) group_code
             << endl;
        delete new_list_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(new_list_contents);

    //
    //  Try and find the right collection
    //

    QPtrListIterator<MetadataCollection> it( metadata_collections );
    MetadataCollection *which_one = NULL;
    MetadataCollection *a_collection;
    while ( (a_collection = it.current()) != 0 )
    {
        ++it;
        if(a_collection->getId() == collection_id)
        {
            which_one = a_collection;
            break;
        }
    }
    
    if(!which_one)
    {
        cerr << "metadataclient.o: asked to parseNewListData(), but "
             << "passed collection id is invalid"
             << endl;
             delete new_list_contents;
             return;
    }

    

    if(update_type)
    {
        //
        //  It's all the items (not a delta)
        //

        which_one->clearAllPlaylists();
    }

    while(rebuilt_internals.size() > 0)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        if(content_code == MarkupCodes::added_list_group)
        {
            QValueVector<char> *list_contents = new QValueVector<char>;
            rebuilt_internals.popGroup(list_contents);
            MdcapInput re_rebuilt_internals(list_contents);
            which_one->addList(re_rebuilt_internals);
            delete list_contents;
        }
        else
        {
            cerr << "metadataclient.o: while doing parseNewListData(), "
                 << "seeing content codes that should not be there"
                 << endl;
            break;
        }
    }

    //
    //  Mark this collections metadata with the right generation number
    //

    which_one->setPlaylistGeneration(collection_generation);

    delete new_list_contents;
}
                                        

MetadataClient::~MetadataClient()
{
    metadata_collections.clear();
}




