/*
    daapinstance.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    An object that knows how to talk to daap servers

*/

#include <vector>
#include <string>
using namespace std;

#include <qdatetime.h>

#include "daapclient.h"
#include "daaprequest.h"
#include "daapresponse.h"


DaapInstance::DaapInstance(
                            MFD *my_mfd,
                            DaapClient *owner, 
                            const QString &l_server_address, 
                            uint l_server_port,
                            const QString &l_service_name
                           )
{
    //
    //  Store the basic info about where to find the daap server this
    //  instance is supposed to talk to
    //
    
    the_mfd = my_mfd;
    parent = owner;
    server_address = l_server_address;
    server_port = l_server_port;
    service_name = l_service_name;
    keep_going = true;
    client_socket_to_daap_server = NULL;
    if(pipe(u_shaped_pipe) < 0)
    {
        warning("could not create a u shaped pipe");
    }

    daap_server_type = DAAP_SERVER_UNKNOWN;
    

    //
    //  Server supplied information:
    //


    //
    //  Data that comes from /server-info
    //

    have_server_info = false;
    supplied_service_name = "";
    server_requires_login = false;
    server_timeout_interval = -1;
    server_supports_autologout = false;
    server_authentication_method = -1;
    server_supports_update = false;
    server_supports_persistentid = false;
    server_supports_extensions = false;
    server_supports_browse = false;
    server_supports_query = false;
    server_supports_index = false;
    server_supports_resolve = false;
    server_numb_databases = -1;
    daap_version.hi = 0;
    daap_version.lo = 0;
    dmap_version.hi = 0;
    dmap_version.lo = 0;

    //
    //  Data that comes from /login
    //

    logged_in = false;    
    session_id = -1;
    
    //
    //  Data that comes from /update
    //
    
    metadata_generation = 1;

    //
    //  Everything else is held in database objects: 
    //

    databases.setAutoDelete(true);
    current_request_db = NULL;
    current_request_playlist = -1;
}                           

void DaapInstance::run()
{

    //
    //  So, first thing we need to do is try and open a client socket to the
    //  daap server
    //
    
    service_details_mutex.lock();
    QHostAddress daap_server_address;
    if(!daap_server_address.setAddress(server_address))
    {
        warning(QString("could not "
                        "set daap server's address "
                        "of %1")
                        .arg(server_address));
        return;
    }

    client_socket_to_daap_server = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_daap_server->setBlocking(false);

    int connect_tries = 0;
    while(!(client_socket_to_daap_server->connect(daap_server_address, 
                                                  server_port)) && keep_going)
    {
        //
        //  Try a few times (it's a non-blocking socket)
        //
        ++connect_tries;
        if(connect_tries > 10)
        {
            warning(QString("could not "
                            "connect to server %1:%2")
                            .arg(server_address)
                            .arg(server_port));
            return;
        }
        usleep(100);
    }

    log(QString("made basic "
                "connection to \"%1\" (%2:%3)")
                .arg(service_name)
                .arg(server_address)
                .arg(server_port), 4);
    

    //
    //  OK, we have a connection, let's send our first request to "prime the
    //  pump"
    //
    
    DaapRequest first_request(this, "/server-info", server_address);
    first_request.send(client_socket_to_daap_server);

    log(QString("sent daap request: \"%1\"")
        .arg(first_request.getRequestString()), 9);
    
    service_details_mutex.unlock();
    
    int a_socket;
    while(keep_going)
    {
    
        //
        //  Just keep checking the server socket to look for data
        //
        
        int nfds = 0;
        fd_set readfds;
        struct timeval timeout;
        
        //
        //  Listen to the server if it's there
        //

        FD_ZERO(&readfds);
        if(client_socket_to_daap_server)
        {
            a_socket = client_socket_to_daap_server->socket();
            if(a_socket > 0)
            {
                FD_SET(a_socket, &readfds);
                if(nfds <= a_socket)
                {
                    nfds = a_socket + 1;
                }
            }
            else
            {
                //
                //  Server disappeared
                //

                log("daap server disappeared ... this instance is terminating", 6);
                    
                keep_going_mutex.lock();
                    keep_going = false;
                keep_going_mutex.unlock();
                a_socket = -1;
                break;
            }
        }
        else
        {
            log("daap server got destroyed (?) ... this client instance is terminating", 6);
                   
            keep_going_mutex.lock();
                keep_going = false;
            keep_going_mutex.unlock();
            a_socket = -1;
            break;
        }
        
        //
        //  Listen on our u-shaped pipe
        //

        FD_SET(u_shaped_pipe[0], &readfds);
        if(nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    

        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int result = select(nfds, &readfds, NULL, NULL, &timeout);
        
        
        if(result < 0)
        {
            warning("got an error from select() "
                    "... not sure what to do ... "
                    "guess I'll exit");
            keep_going_mutex.lock();
                keep_going = false;
            keep_going_mutex.unlock();
            a_socket = -1;
            break;
            
        }
        else
        {
            if(client_socket_to_daap_server)
            {
                a_socket = client_socket_to_daap_server->socket();
                if(a_socket > 0)
                {
                    if(FD_ISSET(a_socket, &readfds))
                    {
                        //
                        //  Excellent ... the daap server is sending us some data.
                        //
                
                        handleIncoming();
                    }
                }
                else
                {

                    //
                    //  Our daap server disappeared on us
                    //
                    
                    log("daap server seems to have disappeared, this instance will exit", 6);
                    keep_going_mutex.lock();
                        keep_going = false;
                    keep_going_mutex.unlock();
                    a_socket = -1;
                    break;
                    
                }
            }
            else
            {
                log("something deleted our socket so this instance will exit",6);
                keep_going_mutex.lock();
                    keep_going = false;
                keep_going_mutex.unlock();
                a_socket = -1;
                break;
            }
            if(FD_ISSET(u_shaped_pipe[0], &readfds))
            {
                u_shaped_pipe_mutex.lock();
                    char read_back[2049];
                    read(u_shaped_pipe[0], read_back, 2048);
                u_shaped_pipe_mutex.unlock();
            }
        }
    }
    
    
    //
    //  If we're logged in, send a logoff (just being polite)
    //  But don't bother waiting for a response
    //
    
    if(logged_in && a_socket > 0)
    {
        DaapRequest logout_request(this, "/logout", server_address);
        logout_request.addGetVariable("session-id", session_id);
        logout_request.send(client_socket_to_daap_server);
        log(QString("sent daap request: \"%1\"")
            .arg(first_request.getRequestString()), 9);
    }
    
    //
    //  Kill off the socket
    //

    if(client_socket_to_daap_server)
    {
        delete client_socket_to_daap_server;
        client_socket_to_daap_server = NULL;
    }
    
    //
    //  Remove any metadata we are responsible for by deleting the database
    //  objects
    //
    
    databases.clear();
    
}

void DaapInstance::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
    wakeUp();
}

bool DaapInstance::keepGoing()
{
    bool return_value = true;
    keep_going_mutex.lock();
        return_value = keep_going;
    keep_going_mutex.unlock();
    return return_value;
}

void DaapInstance::wakeUp()
{
    u_shaped_pipe_mutex.lock();
        write(u_shaped_pipe[1], "wakeup\0", 7);
    u_shaped_pipe_mutex.unlock();
}

void DaapInstance::log(const QString &log_message, int verbosity)
{
    parent->log(log_message, verbosity);
}

void DaapInstance::warning(const QString &warn_message)
{
    parent->warning(warn_message);
}

void DaapInstance::handleIncoming()
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

    DaapResponse *new_response = NULL;
    
    length = client_socket_to_daap_server->readBlock(incoming, buffer_size);
    
    //
    //  If we get nothing back check that the server is actually still there
    //
    
    if(length == 0)
    {
        bool still_there = true;
        if(client_socket_to_daap_server->waitForMore(30, &still_there) < 1)
        {
            if(!still_there)
            {
                //
                //  Server has gone away. Well that sucks!
                //
                
                log("while doing waitForMore(), socket to the "
                    "server seems to have disappeared", 3);
                delete client_socket_to_daap_server;
                client_socket_to_daap_server = NULL;
                return;
            }
        }
    }
    
    
    while(length > 0)
    {
        if(!new_response)
        {
            new_response = new DaapResponse(this, incoming, length);
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
                    
                    if(client_socket_to_daap_server->waitForMore(1000, &server_still_there) < 1)
                    {
                        if(!server_still_there)
                        {
                            new_response->allIsWell(false);
                            wait_a_while = false;
                            length = 0;
                        }
                    }
                    else if(!keep_going)
                    {
                        new_response->allIsWell(false);
                        wait_a_while = false;
                        length = 0;
                    }
                    else
                    {
                        length = client_socket_to_daap_server->readBlock(incoming, buffer_size);
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
            //  Well, something is borked. Give up on this daap server.
            //
            
            service_details_mutex.lock();
            warning(QString("given up on "
                            "\"%1\" (%2:%3)")
                            .arg(service_name)
                            .arg(server_address)
                            .arg(server_port));
            delete client_socket_to_daap_server;
            client_socket_to_daap_server = NULL;
            service_details_mutex.unlock();
        }
    }
    
}

bool DaapInstance::checkServerType(const QString &server_description)
{
    if(have_server_info)
    {
        //
        //  We already know (from previous daap responses) what kind iof
        //  server we're talking to ... so we just make sure it hasn't
        //  suddenly changed.
        //
        
        bool copasetic = true;
        
        if(server_description.left(9) == "iTunes/4.")
        {
            QString sub_version_string = server_description.section(" ", 0, 0);
            sub_version_string = sub_version_string.section("/4.", 1, 1);

            if(sub_version_string.contains('.'))
            {
                //
                //  Ignore sub-sub version, if it is present
                //
                
                sub_version_string = sub_version_string.section(".", 0, 0);
            }
            
            
            bool ok = true;
            int itunes_sub_version = sub_version_string.toInt(&ok);
            if(itunes_sub_version == 1)
            {
                if(daap_server_type != DAAP_SERVER_ITUNES41)
                {
                    copasetic = false;
                }
            }
            else if(itunes_sub_version == 2)
            {
                if(daap_server_type != DAAP_SERVER_ITUNES42)
                {
                    copasetic = false;
                }
            }
            else if(itunes_sub_version == 3)
            {
                if(daap_server_type != DAAP_SERVER_ITUNES43)
                {
                    copasetic = false;
                }
            }
            else if(itunes_sub_version == 5)
            {
                if(daap_server_type != DAAP_SERVER_ITUNES45)
                {
                    copasetic = false;
                }
            }
            else if(itunes_sub_version == 6)
            {
                if(daap_server_type != DAAP_SERVER_ITUNES46)
                {
                    copasetic = false;
                }
            }
            else if(itunes_sub_version == 7)
            {
                if(daap_server_type != DAAP_SERVER_ITUNES47)
                {
                    copasetic = false;
                }
            }
            else if(itunes_sub_version == 8)
            {
                if(daap_server_type != DAAP_SERVER_ITUNES48)
                {
                    copasetic = false;
                }
            }
            else
            {
                if(daap_server_type != DAAP_SERVER_ITUNES4X)
                {
                    copasetic = false;
                }
            }
        }
        else if(server_description.left(6) == "MythTV")
        {
            if(daap_server_type != DAAP_SERVER_MYTH)
            {
                copasetic = false;
            }
        }
        else
        {
            if(daap_server_type != DAAP_SERVER_UNKNOWN)
            {
                copasetic = false;
            }
        }
        if(!copasetic)
        {
            warning(QString("completely confused about "
                            "what kind of server it is talking to: \"%1\"")
                            .arg(server_description));
            
        }
        
    }
    else
    {
        if(server_description.left(9) == "iTunes/4.")
        {
            service_details_mutex.lock();
            daap_server_type = DAAP_SERVER_ITUNES4X;
            QString sub_version_string = server_description.section(" ", 0, 0);
            sub_version_string = sub_version_string.section("/4.", 1, 1);
            
            if(sub_version_string.contains('.'))
            {
                //
                //  Ignore sub-sub version, if it is present
                //
                
                sub_version_string = sub_version_string.section(".", 0, 0);
            }
            
            bool ok = true;
            int itunes_sub_version = sub_version_string.toInt(&ok);
            if(!ok)
            {
                warning("could not determine iTunes minor version number");
            }
            else
            {
                if(itunes_sub_version == 1)
                {
                    daap_server_type = DAAP_SERVER_ITUNES41;
                }
                else if(itunes_sub_version == 2)
                {
                    daap_server_type = DAAP_SERVER_ITUNES42;
                }
                else if(itunes_sub_version == 3)
                {
                    daap_server_type = DAAP_SERVER_ITUNES43;
                }
                else if(itunes_sub_version == 5)
                {
                    daap_server_type = DAAP_SERVER_ITUNES45;
                }
                else if(itunes_sub_version == 6)
                {
                    daap_server_type = DAAP_SERVER_ITUNES46;
                }
                else if(itunes_sub_version == 7)
                {
                    daap_server_type = DAAP_SERVER_ITUNES47;
                }
                else if(itunes_sub_version == 8)
                {
                    daap_server_type = DAAP_SERVER_ITUNES48;
                }
                else
                {
                    warning(QString("seems to be a new version of "
                            "iTunes I don't know about: %1")
                            .arg(itunes_sub_version));
                }                
                
                log(QString("discovered service "
                        "named \"%1\" is being served by "
                        "iTunes version 4.%2")
                        .arg(service_name)
                        .arg(itunes_sub_version), 2);
            }
            service_details_mutex.unlock();
            
            //
            //  If we don't have libopen daap, return false, cause that
            //  means we can't talk to this server
            //
            
            if(the_mfd->getPluginManager()->haveLibOpenDaap())
            {
                return true;
            }
            return false;
            
        }
        else if(server_description.left(6) == "MythTV")
        {
            service_details_mutex.lock();
            daap_server_type = DAAP_SERVER_MYTH;
            log(QString("discovered service "
                        "named \"%1\" is being served by "
                        "another myth box :-)")
                        .arg(service_name), 2);
            service_details_mutex.unlock();
        }
        else
        {
            daap_server_type = DAAP_SERVER_UNKNOWN;
            warning(QString("did not recognize the "
                            "type of server it's connected to: \"%1\"")
                            .arg(server_description));
        }
    }
    return true;
}

void DaapInstance::processResponse(DaapResponse *daap_response)
{
    
    //
    //  Check server type
    //


    if(!checkServerType(daap_response->getHeader("DAAP-Server")))
    {
        warning("I can't talk to this server, "
                "installing libopendaap might help");
        delete client_socket_to_daap_server;     
        client_socket_to_daap_server = NULL;    
        return;
    }

    //
    //  Sanity check ... is the payload xdmap-tagged ?
    //
    
    if(
        daap_response->getHeader("Content-Type") != 
        "application/x-dmap-tagged"
      )
    {
        warning("got a daap response which "
                "is not x-dmap-tagged ... this is bad");
        return;
    }
    
    //
    //  Get a reference to the payload, and make a u8 version of it. Why? 
    //  Because libdaap needs a u8 to create a TagInput object, and I'm too
    //  stupid to figure out a smarter way to do this. 
    //
    
    std::vector<char> *daap_payload = daap_response->getPayload();
    u8 *daap_payload_u8 = new u8[daap_payload->size()];

    std::vector<char>::iterator iter;
    int i = 0;
    for(
        iter = daap_payload->begin() ;
        iter != daap_payload->end() ;
        ++iter
       )
    {
        daap_payload_u8[i] = (*iter);
        ++i;
    }

    Chunk raw_dmap_data(daap_payload_u8, daap_payload->size());
 
    
    //
    //  OK we have the raw payload data in a format that we can construct a
    //  TagInput object from (TagInput is part of daaplib, up in the
    //  daapserver directory). Make the TagInput object.
    //

    TagInput dmap_payload(raw_dmap_data);
    
    //
    //  Pull off the "outside container" (top level) of this data, and
    //  decide what to based on what kind of response it is. We also
    //  "rebuild" the data inside the top level tag into a well formed
    //  TagInput
    //


    Tag top_level_tag;
    Chunk internal_data;
    dmap_payload >> top_level_tag >> internal_data >> end;
    TagInput rebuilt_internal(internal_data);
            

    if(top_level_tag.type == 'msrv')
    {
        doServerInfoResponse(rebuilt_internal);
        return;
    }
    else if(top_level_tag.type == 'mlog')
    {
        doLoginResponse(rebuilt_internal);
        return;
    }
    else if(top_level_tag.type == 'mupd')
    {
        doUpdateResponse(rebuilt_internal);
        return;
    }
    else if(top_level_tag.type == 'avdb')
    {
        doDatabaseListResponse(rebuilt_internal);
    }
    else if(top_level_tag.type == 'adbs')
    {
        if(current_request_db)
        {
            current_request_db->doDatabaseItemsResponse(rebuilt_internal);            
        }
        else
        {
            warning("got an adbs response, but "
                    "have no current database ");
        }
    }
    else if(top_level_tag.type == 'aply')
    {
        if(current_request_db)
        {
            current_request_db->doDatabaseListPlaylistsResponse(rebuilt_internal, metadata_generation);            
        }
        else
        {
            warning("got an aply response, but "
                    "have no current database ");
        }
    }
    else if(top_level_tag.type == 'apso')
    {
        if(current_request_db &&
           current_request_playlist != -1)
        {
            current_request_db->doDatabasePlaylistResponse(
                                                            rebuilt_internal, 
                                                            current_request_playlist,
                                                            metadata_generation
                                                          );            
        }
        else
        {
            warning("got an apso response, but "
                    "have no current database "
                    "and/or current playlist");
        }
    }
    else
    {
        warning("got a top level "
                "unknown tag in a dmap payload ");
    }

    //
    //  OK, something came through and was processed, see if there's
    //  anything our databases want to know? Note that we only send out one
    //  of these at a time ...
    //
    

    Database *a_database = NULL;
    for ( a_database = databases.first(); a_database; a_database = databases.next() )
    {
        if(!a_database->hasItems())
        {
            int which_database = a_database->getDaapId();
            QString request_string = QString("/databases/%1/items").arg(which_database);
            DaapRequest database_request(
                                            this, 
                                            request_string, 
                                            server_address, 
                                            daap_server_type, 
                                            parent->getMfd()->getPluginManager()
                                        );
            database_request.addGetVariable("session-id", session_id);
            
            int generation_delta = a_database->getKnownGeneration();
            if(generation_delta > 0)
            {
                database_request.addGetVariable("delta", generation_delta);
                database_request.addGetVariable("revision-number", metadata_generation);
            }
            else
            {
                database_request.addGetVariable("revision-number", metadata_generation);
            }
            
            //
            //  We have to add a meta GET variable listing all the
            //  information (content codes) about songs that we want to get
            //  back
            //
            
            database_request.addGetVariable("meta", 
                                          QString(
                                                    "dmap.itemname,"
                                                    "dmap.itemid,"
                                                    "daap.songalbum,"
                                                    "daap.songartist,"
                                                    "daap.songbeatsperminute,"
                                                    "daap.songbitrate,"
                                                    "daap.songcomment,"
                                                    "daap.songcompilation,"
                                                    "daap.songcomposer,"
                                                    "daap.songdateadded,"
                                                    "daap.songdatemodified,"
                                                    "daap.songdisccount,"
                                                    "daap.songdiscnumber,"
                                                    "daap.songdisabled,"
                                                    "daap.songeqpreset,"
                                                    "daap.songformat,"
                                                    "daap.songgenre,"
                                                    "daap.songdescription,"
                                                    "daap.songrelativevolume,"
                                                    "daap.songsamplerate,"
                                                    "daap.songsize,"
                                                    "daap.songstarttime,"
                                                    "daap.songstoptime,"
                                                    "daap.songtime,"
                                                    "daap.songtrackcount,"
                                                    "daap.songtracknumber,"
                                                    "daap.songuserrating,"
                                                    "daap.songyear,"
                                                    "daap.songdatakind,"
                                                    "daap.songdataurl"
                                                 ));

            current_request_db = a_database;            
            database_request.send(client_socket_to_daap_server, true);
            log(QString("sent daap request: \"%1\"")
                .arg(database_request.getRequestString()), 9);
            
            //
            //  We've already sent a request, don't send another one
            //

            a_database = databases.last();
        }
        else if(!a_database->hasPlaylistList())
        {
            int which_database = a_database->getDaapId();
            QString request_string = QString("/databases/%1/containers").arg(which_database);
            DaapRequest database_request(
                                            this, 
                                            request_string, 
                                            server_address, 
                                            daap_server_type,
                                            parent->getMfd()->getPluginManager()
                                        );
            database_request.addGetVariable("session-id", session_id);

            int generation_delta = a_database->getKnownGeneration();
            if(generation_delta > 0)
            {
                database_request.addGetVariable("delta", generation_delta);
                database_request.addGetVariable("revision-number", metadata_generation);
            }
            else
            {
                database_request.addGetVariable("revision-number", metadata_generation);
            }
            

            
            current_request_db = a_database;            
            database_request.send(client_socket_to_daap_server, true);
            log(QString("sent daap request: \"%1\"")
                .arg(database_request.getRequestString()), 9);
            
            //
            //  We've already sent a request, don't send another one
            //

            a_database = databases.last();
        }
        else if(!a_database->hasPlaylists())
        {
            int which_database = a_database->getDaapId();
            current_request_playlist = a_database->getFirstPlaylistWithoutList();

            QString request_string = QString("/databases/%1/containers/%2/items")
                                    .arg(which_database)
                                    .arg(current_request_playlist);

            DaapRequest database_request(
                                            this, 
                                            request_string, 
                                            server_address, 
                                            daap_server_type,
                                            parent->getMfd()->getPluginManager()
                                        );

            database_request.addGetVariable("session-id", session_id);

            int generation_delta = a_database->getKnownGeneration();
            if(generation_delta > 0)
            {
                database_request.addGetVariable("delta", generation_delta);
                database_request.addGetVariable("revision-number", metadata_generation);
            }
            else
            {
                database_request.addGetVariable("revision-number", metadata_generation);
            }
            
            
            current_request_db = a_database;            
            database_request.send(client_socket_to_daap_server, true);
            log(QString("sent daap request: \"%1\"")
                .arg(database_request.getRequestString()), 9);
            
            //
            //  We've already sent a request, don't send another one
            //

            a_database = databases.last();
            
            
        }
        else
        {
            
            current_request_db = NULL;
            current_request_playlist = -1;
            
            //
            //  If there's nothing to send, do a hanging update request
            //

            DaapRequest update_request(
                                        this, 
                                        "/update", 
                                        server_address, 
                                        daap_server_type,
                                        parent->getMfd()->getPluginManager()
                                      );
            update_request.addGetVariable("session-id", session_id);
            update_request.addGetVariable("revision-number", metadata_generation);
            update_request.send(client_socket_to_daap_server, true);
            log(QString("sent daap request: \"%1\"")
                .arg(update_request.getRequestString()), 9);
        }
    }
}


void DaapInstance::doServerInfoResponse(TagInput& dmap_data)
{


    Tag a_tag;
    Chunk emergency_throwaway_chunk;

    while(!dmap_data.isFinished())
    {
        //
        //  Go through all the tags and update our info about the server
        //  appropriately. This is not pretty to look at, but the comments
        //  help describe what is going on
        //

        dmap_data >> a_tag;
        std::string a_string;
        u32 a_u32_variable;
        u8  a_u8_variable;
        
        switch(a_tag.type)
        {
            case 'mstt':

                //
                //  A daap status value
                //
                
                dmap_data >> a_u32_variable;
                if(a_u32_variable != 200)    // DAAP ok magic, like HTTP 200
                {
                    warning("got non 200 for DAAP status");
                }
                break;

            case 'mpro':
            
                //
                //  DMAP version number
                //
                
                dmap_data >> dmap_version;
                break;
                    
            case 'apro':

                //
                //  DAAP version numbers
                //
                
                dmap_data >> daap_version;
                break;

            case 'minm':
            
                //
                //  Supplied service name, usually same as service name
                //  published by zeronfig
                //
                
                dmap_data >> a_string;
                {
                    QString q_string = a_string.c_str();
                    service_details_mutex.lock();
                    if(q_string != service_name)
                    {
                        warning(QString("got conflicting names for "
                                        "service: \"%1\" versus \"%2\" (sticking "
                                        "with \"%3\")")
                                        .arg(service_name)
                                        .arg(q_string)
                                        .arg(service_name));
                    }
                    service_details_mutex.unlock();
                }
                break;
                
            case 'mslr':
            
                //
                //  Login required? We throw away the value passed, the
                //  point is that the flag is present.
                //
                
                dmap_data >> a_u8_variable;
                server_requires_login = true;
                break;
                
            case 'mstm':
            
                //
                //  Server timeout interval (how many seconds? microseconds?
                //  it will wait before deciding something has timed out).
                //  Dunno. Save it though.
                //
                
                dmap_data >> a_u32_variable;
                server_timeout_interval = (bool) a_u32_variable;
                break;
                
            case 'msal':
            
                //
                //  Server supports auto logout?
                //
                
                dmap_data >> a_u8_variable;
                server_supports_autologout = true;
                break;
                
            case 'msau':
            
                //
                //  Authentication method. Only ever seen 2 here. Not a clue
                // 
                //
                
                dmap_data >> a_u32_variable;
                server_authentication_method = a_u32_variable;
                break;
                
            case 'msup':
            
                //
                //  Server supports update
                //
                
                dmap_data >> a_u8_variable;
                server_supports_update = true;
                break;
            
            case 'mspi':
            
                dmap_data >> a_u8_variable;
                server_supports_persistentid = true;
                break;
                
            case 'msex':
            
                //
                //  server supports extensions ... don't really have any
                //  idea what that means?
                //

                dmap_data >> a_u8_variable;
                server_supports_extensions = true;
                break;

            case 'msbr':
            
                //
                //  server supports browse (?)
                //                                

                dmap_data >> a_u8_variable;
                server_supports_browse = true;
                break;

            case 'msqy':
            
                //
                //  server supports query
                //

                dmap_data >> a_u8_variable;
                server_supports_query = true;
                break;
                
            case 'msix':
            
                //
                //  server supports index
                //

                dmap_data >> a_u8_variable;
                server_supports_index = true;
                break;
                
            case 'msrs':

                //
                //  server supports resolve
                //

                dmap_data >> a_u8_variable;
                server_supports_resolve = true;
                break;
                
            case 'msdc':
            
                //
                //  Rather important, the number of databases the server has
                //
                
                dmap_data >> a_u32_variable;
                server_numb_databases = (int) a_u32_variable;
                break;

            case 'aeSV':

                //
                //  iTunes music-sharing version, which we ignore
                //
                
                dmap_data >> a_u32_variable;
                break;
                
            case 'msas':
            
                //
                //  DMAP authenticationschemes (another mystery to be ignored)
                //
                
                dmap_data >> a_u32_variable;
                break;
                
            default:
                warning(QString("got an unknown tag type (%1) "
                        "while doing doServerInfoResponse()")
                        .arg(a_tag.type));
                dmap_data >> emergency_throwaway_chunk;
        }

        dmap_data >> end;

    }

    //
    //  Well, I think we're done with that .... make note of the fact that
    //  we have recieved server-info and then try a /login ?
    //  
    
    have_server_info = true;

    DaapRequest login_request(this, "/login", server_address, daap_server_type);
    login_request.send(client_socket_to_daap_server);
    log(QString("sent daap request: \"%1\"")
        .arg(login_request.getRequestString()), 9);
    
}


void DaapInstance::doLoginResponse(TagInput& dmap_data)
{
    bool login_status = false;
    Tag a_tag;
    Chunk emergency_throwaway_chunk;

    while(!dmap_data.isFinished())
    {
        //
        //  parse responses to a /login request
        //

        dmap_data >> a_tag;

        u32 a_u32_variable;

        switch(a_tag.type)
        {
            case 'mstt':

                //
                //  status of login request
                //
                
                dmap_data >> a_u32_variable;
                if(a_u32_variable == 200)    // like HTTP 200 (OK!)
                {
                    login_status = true;
                }
                break;

            case 'mlid':

                //
                //  session id ... important, we definitely need this
                //
                
                dmap_data >> a_u32_variable;
                session_id = (int) a_u32_variable;
                break;

            default:
                warning("got an unknown tag type "
                        "while doing doLoginResponse()");
                dmap_data >> emergency_throwaway_chunk;
        }

        dmap_data >> end;

    }

    if(session_id != -1 && login_status)
    {
        logged_in = true;
        service_details_mutex.lock();
        log(QString("managed to log "
                    "on to \"%1\" and will start loading "
                    "metadata ... ")
                    .arg(service_name), 5);
        service_details_mutex.unlock();

        //
        //  Time to use our new session id to get ourselves an /update
        //
        
        DaapRequest update_request(
                                    this, 
                                    "/update", 
                                    server_address, 
                                    daap_server_type,
                                    parent->getMfd()->getPluginManager()
                                  );
        update_request.addGetVariable("session-id", session_id);
        update_request.addGetVariable("revision-number", metadata_generation);
        update_request.send(client_socket_to_daap_server, true);
        log(QString("sent daap request: \"%1\"")
            .arg(update_request.getRequestString()), 9);
                
        
    }
    else
    {
        service_details_mutex.lock();
        warning(QString("could not log on "
                        "to \"%1\" (password protected?) "
                        "and is giving up")
                        .arg(service_name));
        service_details_mutex.unlock();
    }  
}

void DaapInstance::doUpdateResponse(TagInput& dmap_data)
{
    //
    //  This is telling us either about the initial state of the data or
    //  some revision
    //

    bool update_status = false;
    int new_metadata_generation = metadata_generation;
    Tag a_tag;
    Chunk emergency_throwaway_chunk;

    while(!dmap_data.isFinished())
    {
        //
        //  parse responses to a /login request
        //

        dmap_data >> a_tag;

        u32 a_u32_variable;

        switch(a_tag.type)
        {
            case 'mstt':

                //
                //  status of update request
                //
                
                dmap_data >> a_u32_variable;
                if(a_u32_variable == 200)    // like HTTP 200 (OK!)
                {
                    update_status = true;
                }
                break;

            case 'musr':

                //
                //  the database version (metadata generation on the server
                //  end)
                //
                
                dmap_data >> a_u32_variable;
                new_metadata_generation = (int) a_u32_variable;
                break;

            default:
                warning("got an unknown tag type "
                        "while doing doUpdateResponse()");
                dmap_data >> emergency_throwaway_chunk;
        }

        dmap_data >> end;

    }
    
    if(update_status)
    {
        
        //
        //  Tell our databases to be ignorant 
        //
        
        Database *a_database = NULL;
        for ( a_database = databases.first(); a_database; a_database = databases.next() )
        {   
            a_database->beIgnorant();
        }
    
        
        //
        //  time of see what kind of databases are available
        //
        
        service_details_mutex.lock();
        log(QString("now figured out that "
                    "\"%1\" (%2:%3) is on generation %4")
                    .arg(service_name)
                    .arg(server_address)
                    .arg(server_port)
                    .arg(new_metadata_generation), 4);
        
        DaapRequest database_request(
                                        this, 
                                        "/databases", 
                                        server_address, 
                                        daap_server_type,
                                        parent->getMfd()->getPluginManager()
                                    );
        database_request.addGetVariable("session-id", session_id);
        database_request.addGetVariable("revision-number", new_metadata_generation);
        service_details_mutex.unlock();

        if(metadata_generation > 1)
        {
            database_request.addGetVariable("delta", metadata_generation);
        }


        database_request.send(client_socket_to_daap_server, true);
        log(QString("sent daap request: \"%1\"")
            .arg(database_request.getRequestString()), 9);
        
        metadata_generation = new_metadata_generation;
        
    }
    else
    {
        //
        //  Crap ... 
        //

        warning("doUpdateResponse() got a bad update status");
    }
}

void DaapInstance::doDatabaseListResponse(TagInput& dmap_data)
{

    Tag a_tag;
    Chunk a_chunk;
    
    bool database_list_status = false;
    
    int new_numb_databases = 0;
    int new_received_count_of_databases = 0;

    while(!dmap_data.isFinished())
    {
        //
        //  parse responses to a /database request
        //

        dmap_data >> a_tag;

        u32 a_u32_variable;
        u8  a_u8_variable;

        switch(a_tag.type)
        {
            case 'mstt':

                //
                //  status of update request
                //
                
                dmap_data >> a_u32_variable;
                if(a_u32_variable == 200)    // like HTTP 200 (OK!)
                {
                    database_list_status = true;
                }
                break;

            case 'muty':
            
                //
                //  update type ... only ever seen 0 here ... dunno ?
                //
                
                dmap_data >> a_u8_variable;
                break;
                
            case 'mtco':
                
                //
                //  number of databases
                //
                
                dmap_data >> a_u32_variable;
                new_numb_databases = a_u32_variable;
                break;
                
            case 'mrco':
            
                //
                //  received count of databases (how many of them are
                //  described in this datastream)
                //                
                
                dmap_data >> a_u32_variable;
                new_received_count_of_databases = a_u32_variable;
                break;
                
            case 'mlcl':

                //
                //  This is "listing" tag, saying there's a list of other tags to come
                //
                
                dmap_data >> a_chunk;
                {
                    TagInput re_rebuilt_internal(a_chunk);
                    parseDatabaseListings(re_rebuilt_internal, new_received_count_of_databases);
                }
                break;

            default:
                warning("got an unknown tag type "
                        "while doing doDatabaseListResponse()");
                dmap_data >> a_chunk;
        }

        dmap_data >> end;

    }
}    

void DaapInstance::parseDatabaseListings(TagInput& dmap_data, int how_many)
{
    Tag a_tag;
    u32 a_u32_variable;
    u64 a_u64_variable;
    std::string a_string;
    Chunk listing;
    
    
    for(int i = 0; i < how_many; i++)
    {
        int new_database_id = -1;
        int new_database_persistent_id = 1;
        QString new_database_name = "";
        int new_database_expected_item_count = -1;
        int new_database_expected_container_count = -1;
        Chunk emergency_throwaway_chunk;

        dmap_data >> a_tag >> listing >> end;
    
        if(a_tag.type != 'mlit')
        {
            //
            //  this should not happen
            //
            warning("got a non mlit tag "
                    "where one absolutely has to be.");
        }
        
        TagInput internal_listing(listing);
        while(!internal_listing.isFinished())
        {
    
            internal_listing >> a_tag;
        
            switch(a_tag.type)
            {
                case 'miid':
                
                    //
                    //  ID for this database
                    //

                    internal_listing >> a_u32_variable;
                    new_database_id = a_u32_variable;
                    break;

                case 'mper':
            
                    //
                    //  persistent id for this database
                    //
                
                    internal_listing >> a_u64_variable;
                    new_database_persistent_id = a_u64_variable;
                    break;
                
                case 'minm':
            
                    //
                    //  name for this database (usually same as service name if
                    //  there is only one)
                    //
                
                    internal_listing >> a_string;
                    new_database_name = QString::fromUtf8(a_string.c_str());
                    break;

                case 'mimc':
            
                    //
                    //  item count (number of songs)
                    //
                
                    internal_listing >> a_u32_variable;
                    new_database_expected_item_count = a_u32_variable;
                    break;
                
                case 'mctc':
            
                    //
                    //  container count (number of playlists)
                    //
                
                    internal_listing >> a_u32_variable;
                    new_database_expected_container_count = a_u32_variable;
                    break;

                default:
                    
                    warning("unknown tag while parsing database listing");
                    internal_listing >> emergency_throwaway_chunk;
            }
            internal_listing >> end;
        }    

        //
        //  Create a new database for this ... if we don't have one for this already
        //
            
        Database *a_database = NULL;
        bool need_new_database = true;
        for ( a_database = databases.first(); a_database; a_database = databases.next() )
        {
            if(a_database->getDaapId() == new_database_id)
            {
                need_new_database = false;
                break;
            }
        }

        if(need_new_database)
        {
            service_details_mutex.lock();
            Database *new_database = new Database(
                                                    new_database_id,
                                                    new_database_name,
                                                    new_database_expected_item_count,
                                                    new_database_expected_container_count,
                                                    the_mfd,
                                                    parent,
                                                    session_id,
                                                    server_address,
                                                    server_port,
                                                    daap_server_type
                                             );
            service_details_mutex.unlock();
            log(QString("creating new database with daap server id of %1")
                .arg(new_database_id), 9);
            databases.append(new_database);
        }
    }
}

bool DaapInstance::isThisYou(QString a_name, QString an_address, uint a_port)
{
    bool return_value = false;
    service_details_mutex.lock();
        if(
            a_name == service_name &&
            an_address == server_address &&
            a_port == server_port
          )
        {
            return_value = true;
        }
    service_details_mutex.unlock();
    return return_value;
}

DaapInstance::~DaapInstance()
{
    databases.clear();
}

