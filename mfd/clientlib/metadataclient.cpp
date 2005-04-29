/*
	metadataclient.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's metadata service

*/

#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qdatetime.h>

#include <mythtv/generictree.h>

#include "metadataclient.h"
#include "mfdinterface.h"
#include "mfdcontent.h"

#include "mdcaprequest.h"
#include "mdcapresponse.h"
#include "events.h"

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
                            l_port,
                            "metadata"
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

void MetadataClient::commitListEdits(
                                        UIListGenericTree *playlist_tree,
                                        bool new_playlist,
                                        QString playlist_name
                                    )
{
    //
    //  We need to build an mdcap out request with the new playlist properly
    //  marked up as it's payload
    //


    //
    //  Create a mdcap output request object with the correct URL
    //
    
    QString commit_url = QString("/commit/%1/list/%2/")
                                .arg(playlist_tree->getAttribute(0))
                                .arg(playlist_tree->getInt());

    MdcapRequest commit_request(commit_url, ip_address);
    commit_request.addGetVariable("session-id", session_id);


    MdcapOutput outgoing_payload;
    
    outgoing_payload.addCommitListGroup();

        outgoing_payload.addCollectionId(playlist_tree->getAttribute(0));
        outgoing_payload.addListId(playlist_tree->getInt());
        outgoing_payload.addListName(playlist_name);
        if(new_playlist)
        {
            outgoing_payload.addCommitListType(true);
        }    
        else
        {
            outgoing_payload.addCommitListType(false);
        }

        outgoing_payload.addCommitListGroupList();

            QPtrList<GenericTree> *all_children = playlist_tree->getAllChildren();
            QPtrListIterator<GenericTree> it(*all_children);
            GenericTree *child;
            while ((child = it.current()) != 0)
            {
                outgoing_payload.addListItem(child->getInt() * child->getAttribute(3));
                ++it;
            }    

        outgoing_payload.endGroup();

    outgoing_payload.endGroup();

    commit_request.setPayload(outgoing_payload.getContents());

    commit_request.send(client_socket_to_service);

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
            //  This could be that some _other_ mfd went away, taking it's
            //  metadata with it while we were in the middle of asking
            //  _this_ mfd about it. Try and recover by going back to basics
            //

            cerr << "got an error on a request, trying to recover" << endl;

            delete new_response;
            new_response = NULL;
            metadata_collections.clear();
            MdcapRequest update_request("/update", ip_address);
            update_request.addGetVariable("session-id", session_id);
            update_request.send(client_socket_to_service);
            
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
             << "in a mdcap payload ("
             << (int) first_tag
             << ")"
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
        else
        {
            a_collection->beUpToDate();
        }
        ++it;
    }
    
    //
    //  If I'm here, that means everything is up to date. I need to send a
    //  hanging update request (so the server will send me anything new if
    //  it becomes available)
    //
    
    MdcapRequest hanging_update_request("/update", ip_address);
    hanging_update_request.addGetVariable("session-id", session_id);
    
    //
    //  We add an HTTP header that describes the generation of every
    //  collection we know about
    //


    QString generation_header = "MDCAP-Generations: ";
    bool first_one;
    QPtrListIterator<MetadataCollection> hu_it( metadata_collections );
    MetadataCollection *b_collection;
    while ( (b_collection = hu_it.current()) != 0 )
    {
        ++hu_it;
        if(first_one)
        {
            first_one = false;
        }
        else
        {
            generation_header.append(",");
        }
        generation_header.append(
                                    QString("%1/%2")
                                    .arg(b_collection->getId())
                                    .arg(b_collection->getGeneration())
                                );
    }
    hanging_update_request.addHeader(generation_header);
    hanging_update_request.send(client_socket_to_service);
    
    
    //
    //  Debugging output
    //
    
    //printMetadata();
    
    //
    //  Time to build a tree
    //
    
    buildTree();

}

void MetadataClient::parseServerInfo(MdcapInput &mdcap_input)
{
    QValueVector<char> *group_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(group_contents);
    
    if(group_code != MarkupCodes::server_info_group)
    {
        cerr << "metadataclient.o: asked to parseServerInfo(), but "
             << "group code was not server_info_group ("
             << (int) group_code
             << ")"
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
                     << "understand while doing parseServerInfo() ("
                     << (int) content_code
                     << ")"
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
        MetadataCollection *to_delete = findCollection((*away_it));
        if(to_delete)
        {
            metadata_collections.remove(to_delete);
        }
        else
        {
            cerr << "metadataclient.o: wanted to delete a "
                 << "collection that wasn't there" 
                 << endl;   
        }
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
    MetadataCollection *which_collection = NULL;

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
                which_collection = findCollection(new_collection_id);
                if(!which_collection)
                {
                    cerr << "ref to collection id was bad in parseItems()" << endl;
                    all_is_well = false;
                }
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
                                    which_collection, 
                                    new_collection_generation,
                                    new_update_type,
                                    new_total_items,
                                    new_added_items,
                                    new_deleted_items,
                                    rebuilt_internals
                                 );
                break;

            case MarkupCodes::deleted_items_group:
                parseDeletedItemData(
                                        which_collection,
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

    if(all_is_well)
    {
        //
        //  Mark this collections metadata with the right generation number
        //

        if(which_collection)
        {
            which_collection->setMetadataGeneration(new_collection_generation);
            which_collection->setExpectedCount(new_total_items);
            which_collection->setPending(new_collection_generation);
        }
        else
        {
            cerr << "how the hell did you do that?" << endl;
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
    
    MetadataCollection *which_collection = NULL;

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
                which_collection = findCollection(new_collection_id);
                if(!which_collection)
                {
                    cerr << "ref to collection id was bad in parseLists()" << endl;
                    all_is_well = false;
                }
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
                                    which_collection, 
                                    new_collection_generation,
                                    new_update_type,
                                    new_total_items,
                                    new_added_items,
                                    new_deleted_items,
                                    rebuilt_internals
                                );
                break;
                
            case MarkupCodes::deleted_lists_group:
                parseDeletedListData(
                                        which_collection,
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

    if(all_is_well)
    {
        //
        //  Mark this collections playlists with the right generation number
        //

        if(which_collection)
        {
            which_collection->setPlaylistGeneration(new_collection_generation);
            which_collection->setPending(new_collection_generation);
        }
        else
        {
            cerr << "how'd you do that?" << endl;
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
    bool     new_collection_editable = false;
    bool     new_collection_ripable = false;

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
                
            case MarkupCodes::collection_is_editable:
                new_collection_editable = mdcap_input.popCollectionEditable();
                break;
                
            case MarkupCodes::collection_is_ripable:
                new_collection_ripable = mdcap_input.popCollectionRipable();
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
            
            a_collection->setPending(new_collection_generation);
            
            //
            //  Do some sanity checking
            //
            
            if(a_collection->getName() != new_collection_name)
            {
                cerr << "metadatclient.o: collection with id of "
                     << new_collection_id 
                     << " seems to have changed names from \""
                     << a_collection->getName()
                     << "\" to \""
                     << new_collection_name
                     << "\", which is not very good "
                     << endl;
            }
            
            if(a_collection->getType() != (int) new_collection_type)
            {
                cerr << "metadatclient.o: collection with id of "
                     << new_collection_id 
                     << " seems to have changed collection type, "
                     << "which is pretty awful of it"
                     << endl;
            }
            
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

            MetadataCollection *new_collection = 
                new MetadataCollection(
                                        new_collection_id,
                                        new_collection_count,
                                        new_collection_name,
                                        new_collection_generation,
                                        metadata_type
                                       );
            new_collection->setEditable(new_collection_editable);
            new_collection->setRipable(new_collection_ripable);
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
                                        MetadataCollection *which_collection,
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
        which_collection == NULL    ||
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
             << "group code was not add_items_group, it was "
             << (int) group_code
             << endl;
        delete new_item_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(new_item_contents);


    if(update_type)
    {
        //
        //  It's all the items (not a delta)
        //

        which_collection->clearAllMetadata();
    }

    while(rebuilt_internals.size() > 0)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        if(content_code == MarkupCodes::added_item_group)
        {
            QValueVector<char> *item_contents = new QValueVector<char>;
            rebuilt_internals.popGroup(item_contents);
            MdcapInput re_rebuilt_internals(item_contents);
            which_collection->addItem(re_rebuilt_internals);
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

    delete new_item_contents;
}
                                        
void MetadataClient::parseDeletedItemData(
                                            MetadataCollection *which_collection,
                                            MdcapInput &mdcap_input
                                         )
{
    //
    //  Make sure all the incoming stuff is set
    //
    
    if(!which_collection)
    {
        cerr << "metadataclient.o: asked to parseDeletedItemData(), but passed "
             << "invalid container pointer"
             << endl;
        return;
    }
    

    //
    //  Make sure we are dealing with what we think we're dealing with
    //

    QValueVector<char> *del_item_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(del_item_contents);

    if(group_code != MarkupCodes::deleted_items_group)
    {
        cerr << "metadataclient.o: asked to parseDeletedItemData(), but "
             << "group code was not deleted_items_group, it was "
             << (int) group_code
             << endl;
        delete del_item_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(del_item_contents);


    while(rebuilt_internals.size() > 0)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        if(content_code == MarkupCodes::deleted_item)
        {
            uint which_item = rebuilt_internals.popDeletedItem();
            which_collection->deleteItem(which_item);

        }
        else
        {
            cerr << "metadataclient.o: while doing parseDeletedItemData(), "
                 << "seeing content codes that should not be there"
                 << endl;
            break;
        }
    }

    delete del_item_contents;
}
                                        
void MetadataClient::parseNewListData(
                                        MetadataCollection *which_collection,
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
        !which_collection           ||
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

    if(update_type)
    {
        //
        //  It's all the items (not a delta)
        //

        which_collection->clearAllPlaylists();
    }

    while(rebuilt_internals.size() > 0)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        if(content_code == MarkupCodes::added_list_group)
        {
            QValueVector<char> *list_contents = new QValueVector<char>;
            rebuilt_internals.popGroup(list_contents);
            MdcapInput re_rebuilt_internals(list_contents);
            which_collection->addList(re_rebuilt_internals);
            delete list_contents;
        }
        else
        {
            cerr << "metadataclient.o: while doing parseNewListData(), "
                 << "seeing content codes that should not be there ("
                 << (int) content_code
                 << ")"
                 << endl;
            break;
        }
    }

    delete new_list_contents;
}


void MetadataClient::parseDeletedListData(
                                            MetadataCollection *which_collection,
                                            MdcapInput &mdcap_input
                                     )
{

    //
    //  Make sure we have a collection
    //
    
    if(!which_collection)
    {
        cerr << "metadataclient.o: asked to parseDeletedListData(), but passed "
             << "no collection pointer"
             << endl;
        return;
    }
    

    //
    //  Make sure we are dealing with what we think we're dealing with
    //

    QValueVector<char> *del_list_contents = new QValueVector<char>;
    
    char group_code = mdcap_input.popGroup(del_list_contents);

    if(group_code != MarkupCodes::deleted_lists_group)
    {
        cerr << "metadataclient.o: asked to parseDeletedListData(), but "
             << "group code was not deleted_lists_group, it was "
             << (int) group_code
             << endl;
        delete del_list_contents;
        return;
    }
    

    MdcapInput rebuilt_internals(del_list_contents);

    while(rebuilt_internals.size() > 0)
    {
        char content_code = rebuilt_internals.peekAtNextCode();
        if(content_code == MarkupCodes::deleted_list)
        {
            uint which_list = rebuilt_internals.popDeletedList();
            which_collection->deleteList(which_list);
        }
        else
        {
            cerr << "metadataclient.o: while doing parseDeletedListData(), "
                 << "seeing content codes that should not be there ("
                 << (int) content_code
                 << ")"
                 << endl;
            break;
        }
    }

    delete del_list_contents;
}



MetadataCollection* MetadataClient::findCollection(int collection_id)
{
    //
    //  Try and find the right collection
    //

    QPtrListIterator<MetadataCollection> it( metadata_collections );
    MetadataCollection *return_value = NULL;
    MetadataCollection *a_collection;
    while ( (a_collection = it.current()) != 0 )
    {
        ++it;
        if(a_collection->getId() == collection_id)
        {
            return_value = a_collection;
            break;
        }
    }
    
    return return_value;
}


void MetadataClient::printMetadata()
{
    cout << "@@@@@@@@@@@@@@@@@@@ DEBUGGING OUTPUT @@@@@@@@@@@@@@@@@@@@" << endl;
    cout << "mfd client library metadata client connected to "
         << ip_address
         << " has " 
         << metadata_collections.count()
         << " collections. They are:"
         << endl
         << endl;

    QPtrListIterator<MetadataCollection> it( metadata_collections );
    MetadataCollection *a_collection;
    while ( (a_collection = it.current()) != 0 )
    {
        ++it;
        a_collection->printMetadata();
    }
}
                                        

void MetadataClient::buildTree()
{
    QTime collection_build_timer;
    collection_build_timer.start();

    //
    //  Build a complete MfdContentCollection object (all the metadata,
    //  playlists, and a set of UIListGenericTree's, and all of them sorted)
    //  that reflects everything available from this mfd.
    //
    
    MfdContentCollection *new_collection = new MfdContentCollection(
                                                    getId(), 
                                                    mfd_interface->getClientWidth(),
                                                    mfd_interface->getClientHeight()
                                                                   );
    
  
    //
    //  Iterate over all collections
    //
    
    QPtrListIterator<MetadataCollection> it( metadata_collections );
    MetadataCollection *a_collection;
    while ( (a_collection = it.current()) != 0 )
    {
        ++it;
        
        //
        //  Add every item in this collection to the content container
        //
        
        QIntDict<Metadata> *metadata = a_collection->getMetadata();
        QIntDictIterator<Metadata> m_it( *metadata ); 
        for ( ; m_it.current(); ++m_it )
        {
            new_collection->addMetadata(m_it.current(), a_collection->getName(), a_collection);
        }


        //
        //  Add every playlist in this container
        //
        
        QIntDict<ClientPlaylist> *playlist = a_collection->getPlaylists();
        QIntDictIterator<ClientPlaylist> p_it( *playlist ); 
        for ( ; p_it.current(); ++p_it )
        {
            new_collection->addPlaylist(p_it.current(), a_collection);
        }
        
        //
        //  Tally true playlist counts (resolved out to root tracks)
        //
        
        new_collection->tallyPlaylists();
        
        //
        //  If this collection is editable, add ability for user to create
        //  new playlist
        //
        
        if(a_collection->isEditable())
        {
            new_collection->addNewPlaylistAbility(a_collection->getName(), a_collection->getId());
        }
        
        //
        //  Sort everything
        //
        
        new_collection->sort();
    }        

    //
    //  If we're in debugging mode, say how long it took to build all that
    //
    
#ifdef MFD_DEBUG_BUILD
    cout << "metadataclient.o: built new mfd content container in "
         << collection_build_timer.elapsed() / 1000.0
         << " second(s)"
         << endl;
#endif    

    //
    //  Hand control of this collection off to the main execution loop (and never,
    //  _EVER_ touch it again)
    //

    MfdMetadataChangedEvent *mce = new MfdMetadataChangedEvent(mfd_id, new_collection);
    QApplication::postEvent(mfd_interface, mce);
}

MetadataClient::~MetadataClient()
{
    metadata_collections.clear();
}
