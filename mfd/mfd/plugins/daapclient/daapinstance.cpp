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
}                           

void DaapInstance::run()
{
    //
    //  So, first thing we need to do is try and open a client socket to the
    //  daap server
    //
    
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
                                                  server_port)))
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
            if(client_socket_to_daap_server->socket() > 0)
            {
                FD_SET(client_socket_to_daap_server->socket(), &readfds);
                if(nfds <= client_socket_to_daap_server->socket())
                {
                    nfds = client_socket_to_daap_server->socket() + 1;
                }
            }
        }
        
        //
        //  Listen on our u-shaped pipe
        //

        FD_SET(u_shaped_pipe[0], &readfds);
        if(nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    

        
        timeout.tv_sec = 200;
        timeout.tv_usec = 0;
        int result = select(nfds, &readfds, NULL, NULL, &timeout);
        if(result < 0)
        {
            warning("got an error from select() "
                    "... not sure what to do");
        }
        else
        {
            if(client_socket_to_daap_server)
            {
                if(FD_ISSET(client_socket_to_daap_server->socket(), &readfds))
                {
                    //
                    //  Excellent ... the daap server is sending us some data.
                    //
                
                    handleIncoming();
                }
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
    
    if(logged_in)
    {
        DaapRequest logout_request(this, "/logout", server_address);
        logout_request.addGetVariable("session-id", session_id);
        logout_request.send(client_socket_to_daap_server, true);
    }
    
    //
    //  Kill off the socket
    //

    if(client_socket_to_daap_server)
    {
        delete client_socket_to_daap_server;
        client_socket_to_daap_server = NULL;
    }
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
            
            warning(QString("given up on "
                            "\"%1\" (%2:%3)")
                            .arg(service_name)
                            .arg(server_address)
                            .arg(server_port));
            delete client_socket_to_daap_server;
            client_socket_to_daap_server = NULL;
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
        
        if(server_description.left(6) == "iTunes")
        {
            if(daap_server_type != DAAP_SERVER_ITUNESLESSTHAN401)
            {
                copasetic = false;
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
        if(server_description.left(6) == "iTunes")
        {
            //
            //  Figure out if this is 4.0.1 or greater, in which case, we
            //  can't talk to it.
            //
            
            QString itunes_version_string = server_description.section('/', -1, -1);
            itunes_version_string = itunes_version_string.section(' ',0,0);
            bool ok;
            float itunes_version = itunes_version_string.toFloat(&ok);
            if(!ok)
            {
                warning(QString("could not determine iTunes version from: %1")
                                .arg(server_description));
                daap_server_type = DAAP_SERVER_ITUNES401ORGREATER;
                return false;
            }
            else
            {
                if(itunes_version >= 4.1)
                {
                    daap_server_type = DAAP_SERVER_ITUNES401ORGREATER;
                    return false;
                }
                else
                {
                    daap_server_type = DAAP_SERVER_ITUNESLESSTHAN401;
                    log(QString("discovered service "
                                "named \"%1\" is being served by "
                                "iTunes (v < 4.1 !!)")
                                .arg(service_name), 2);
                    return true;
                }
            }
            
        }
        else if(server_description.left(6) == "MythTV")
        {
            daap_server_type = DAAP_SERVER_MYTH;
            log(QString("discovered service "
                        "named \"%1\" is being served by "
                        "another myth box :-)")
                        .arg(service_name), 2);
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
    //  Check the type of server
    //
    
    if(!checkServerType(daap_response->getHeader("DAAP-Server")))
    {
        //
        //  We got an iTunes 4.1 or greater server.
        //
        
        warning("cannot talk to iTunes because\n"
                "                   it uses a totally undocumented Client-DAAP-Validation header\n\n"
                "                   Please complain to Apple at:\n\n"
                "                   Apple Customer Relations, (800) 767-2775\n" 
                "                   http://www.apple.com/feedback/itunes.html\n"
               ); 
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
    for(uint i = 0; i < daap_payload->size(); i++)
    {
        daap_payload_u8[i] = daap_payload->at(i);
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
    }
    else if(top_level_tag.type == 'mlog')
    {
        doLoginResponse(rebuilt_internal);
    }
    else if(top_level_tag.type == 'mupd')
    {
        doUpdateResponse(rebuilt_internal);
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
            DaapRequest update_request(this, request_string, server_address);
            update_request.addGetVariable("session-id", session_id);
            //update_request.addGetVariable("revision-number", metadata_generation);
            
            //
            //  We have to add a meta GET variable listing all the
            //  information (content codes) about songs that we want to get
            //  back
            //
            
            update_request.addGetVariable("meta", 
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
            update_request.send(client_socket_to_daap_server);
            
            //
            //  We've already sent a request, don't send another one
            //

            a_database = databases.last();
        }
        else
        {
            current_request_db = NULL;
        }
    }

    //
    //  If there's nothing to send, do a hanging update request
    //
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
            case 'apro':

                //
                //  DMAP and DAAP version numbers ... which we ignore (!)
                //  for the time being.
                //
                
                dmap_data >> a_u32_variable;
                break;

            case 'minm':
            
                //
                //  Supplied service name, usually same as service name
                //  published by zeronfig
                //
                
                dmap_data >> a_string;
                {
                    QString q_string = QString::fromUtf8(a_string.c_str());
                    if(q_string != service_name)
                    {
                        warning(QString("got conflicting names for "
                                        "service: \"%1\" versus \"%2\" (sticking "
                                        "with \"%3\")")
                                        .arg(service_name)
                                        .arg(q_string)
                                        .arg(service_name));
                    }
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

            default:
                warning("got an unknown tag type "
                        "while doing doServerInfoResponse()");
                dmap_data >> emergency_throwaway_chunk;
        }

        dmap_data >> end;

    }

    //
    //  Well, I think we're done with that .... make note of the fact that
    //  we have recieved server-info and then try a /login ?
    //  
    
    have_server_info = true;

    DaapRequest login_request(this, "/login", server_address);
    login_request.send(client_socket_to_daap_server);
    
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
        log(QString("managed to log "
                    "on to \"%1\" and will start loading "
                    "metadata ... ")
                    .arg(service_name), 5);

        //
        //  Time to use our new session id to get ourselves an /update
        //
        
        DaapRequest update_request(this, "/update", server_address);
        update_request.addGetVariable("session-id", session_id);
        update_request.addGetVariable("revision-number", metadata_generation);
        update_request.send(client_socket_to_daap_server);
                
        
    }
    else
    {
        warning(QString("could not log on "
                        "to \"%1\" (password protected?) "
                        "and is giving up")
                        .arg(service_name));
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
        metadata_generation = new_metadata_generation;
        
        //
        //  time of see what kind of databases are available
        //
        
        log(QString("now figured out that "
                    "\"%1\" (%2:%3) is on generation %4")
                    .arg(service_name)
                    .arg(server_address)
                    .arg(server_port)
                    .arg(metadata_generation), 4);
        
        DaapRequest database_request(this, "/databases", server_address);
        database_request.addGetVariable("session-id", session_id);
        database_request.addGetVariable("revision-number", metadata_generation);
        database_request.send(client_socket_to_daap_server);
        
        
    }
    else
    {
        //
        //  Crap ... 
        //
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
        //  Create a new database for this ...
        //
            
        Database *new_database = new Database(
                                                new_database_id,
                                                new_database_name,
                                                new_database_expected_item_count,
                                                new_database_expected_container_count,
                                                the_mfd,
                                                parent,
                                                session_id,
                                                server_address,
                                                server_port
                                             );
        databases.append(new_database);
    }
}

DaapInstance::~DaapInstance()
{
    databases.clear();
}

