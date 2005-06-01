/*
    daapserver.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    

*/

#include <qapplication.h>
#include <qvaluelist.h>

#include "mfd_events.h"
#include "httpinrequest.h"
#include "httpoutresponse.h"
#include "settings.h"

#include "daapserver.h"
#include "mcc_bitfield.h"


#include "daaplib/registry.h"

DaapServer *daap_server = NULL;


const int   DAAP_OK = 200;


DaapServer::DaapServer(MFD *owner, int identity)
      :MFDHttpPlugin(owner, identity, mfdContext->getNumSetting("mfd_daapserver_port"), "daap server", 2)
{
    metadata_server = owner->getMetadataServer();
    metadata_containers = metadata_server->getMetadataContainers();    

    service_name = QString("MythMusic on %1").arg(hostname); 

    //
    //  Register this (daap) service
    //

    Service *daap_service = new Service(
                                        QString("MythMusic on %1").arg(hostname),
                                        QString("daap"),
                                        hostname,
                                        SLT_HOST,
                                        (uint) port_number
                                       );
 
    ServiceEvent *se = new ServiceEvent( true, true, *daap_service);
    QApplication::postEvent(parent, se);
    delete daap_service;

}


void DaapServer::handleIncoming(HttpInRequest *http_request, int client_id)
{

    /*

    //
    //  Debugging (mostly for new versions of iTunes, when you need to print
    //  out what this server is getting as requests in order to figure out
    //  exactly what the daap*client* plugin should be sending in the other
    //  direction)
    //

    http_request->printRequest();
    http_request->printHeaders();

    */

    //
    //  Create a DaapRequest object that will be built up to understand the
    //  request that has just come in from a client
    //

    DaapRequest *daap_request = new DaapRequest();

    //
    //  Start to build up the request by parsing the path of the request.
    //  This also figures out the User Agent (ie. client is another mythbox,
    //  iTunes, etc.)
    //
    
    parsePath(http_request, daap_request);
    
    //
    //  Make sure we understand the path
    //

    if(daap_request->getRequestType()       == DAAP_REQUEST_NOREQUEST)
    {
        delete daap_request;
        daap_request = NULL;
        return;
    }
    
    //
    //  Send back whatever the client asked for, based on the request type
    //
    
    else if( daap_request->getRequestType() == DAAP_REQUEST_SERVINFO )
    {
        sendServerInfo(http_request, daap_request);
        delete daap_request;
        daap_request = NULL;
        return;
    }   
    
    else if( daap_request->getRequestType() == DAAP_REQUEST_CONTCODES )
    {
        sendContentCodes(http_request);
        delete daap_request;
        daap_request = NULL;
        return;
    }

    else if( daap_request->getRequestType() == DAAP_REQUEST_LOGIN)
    {
        u32 session_id = daap_sessions.getNewId();
        sendLogin( http_request, session_id);
        delete daap_request;
        daap_request = NULL;
        return;
    }


    //
    //  If we've made it this far, then we have a logged in client, so we
    //  should be able to parse the request to get session ID, etc.
    //

    parseVariables( http_request, daap_request);

    if(!daap_sessions.isValid(daap_request->getSessionId()))
    {
        //
        //  Hmmm ... this isn't right. The DAAP client did not ask for
        //  server info or a login, but it does not have a valid session id
        //  (which it can get by requesting a login).
        //
        
        http_request->getResponse()->setError(403); // forbidden
        delete daap_request;
        daap_request = NULL;
        return;
    }
    

    if( daap_request->getRequestType() == DAAP_REQUEST_DATABASES)
    {
        //
        //  The client wants some kind of metatdata about items or
        //  containers available (or it wants a specific item to play). Go
        //  ahead and give it to the client, as long as their version of the
        //  database is the same as our version. UNLESS, the client passes
        //  db version of 0, which for some inexplicable reason, is what
        //  iTunes does when it's actually trying to get a hold of a stream
        //  for playing.
        //

        //
        //  We don't want the metadata to change from this point on (once
        //  we've got the audio generation value). So we lock it, do all our
        //  processing, and then unlock it. This is not as bad as it sounds,
        //  because all we're doing is building an in memory structured
        //  response to the request. Actually sending the bits over the wire
        //  is handled by our base class outside of the lock.
        

        metadata_server->lockMetadata();

            uint audio_generation = metadata_server->getMetadataLocalAudioGeneration();

            if(daap_request->getDatabaseVersion() != audio_generation &&
               daap_request->getDatabaseVersion() != 0)
            {
                log(QString(": a client asked for a database request, "
                            "but has stale db reference "
                            "(mfd db# = %1, client db# = %2)")
                            .arg(audio_generation)
                            .arg(daap_request->getDatabaseVersion()), 9);

                sendUpdate(http_request, audio_generation);
            }
            else
            {
                //
                //  Ah, so we really need to send metadata. 
                //

                sendMetadata(http_request, http_request->getUrl(), daap_request);

            }
        metadata_server->unlockMetadata();
    }
    else if (daap_request->getRequestType() == DAAP_REQUEST_UPDATE)
    {

        //
        //  
        //  should only respond if the database version numbers are out of
        //  whack.
        //


        uint audio_generation = metadata_server->getMetadataLocalAudioGeneration();
        if(daap_request->getDatabaseVersion() != audio_generation)
        {
            log(QString(": a client asked for and will get an update "
                        "(mfd db# = %1, client db# = %2)")
                        .arg(audio_generation)
                        .arg(daap_request->getDatabaseVersion()), 9);
            sendUpdate(http_request, audio_generation);
        }
        else
        {
            //
            //  We can hold this request, so that when (if?) the database of
            //  music does change, we have some way to push back the update
            //
            
            http_request->sendResponse(false);
            if(http_request->getHeader("Connection") == "close")
            {
                warning("got a hanging /update request, but the client set Connection header to close??");
            }
            else
            {
                hanging_updates_mutex.lock();
                    hanging_updates.append(client_id);
                    log(QString("now has %1 client(s) sitting in a hanging update")
                        .arg(hanging_updates.count()), 8);
                hanging_updates_mutex.unlock();
            }
        }
    }
    else if(daap_request->getRequestType() == DAAP_REQUEST_LOGOUT)
    {
        //
        //  Don't do much of anything but say that's just fine
        //
        
        http_request->getResponse()->setError(204);
    }

    
    delete daap_request;
    daap_request = NULL;
    
}


void DaapServer::parsePath(HttpInRequest *http_request, DaapRequest *daap_request)
{
         
    QString the_path = http_request->getUrl();

    //
    //  iTunes 4.5 (and above?) send a fully formed URL rather than a
    //  relative GET request, so munge the_path a bit if it looks like it's
    //  a full URL
    //
    
    if(the_path.contains("daap://"))
    {
        the_path = the_path.section(":",2,2);
        the_path = the_path.section("/", 1, -1);
        the_path = QString("/%1").arg(the_path);
    }
    
   

    //
    //  Figure out what kind of request this is
    //
    
    if(the_path == "/server-info")
    {
        daap_request->setRequestType(DAAP_REQUEST_SERVINFO);
        log(": a client asked for /server-info", 9);
    }
    else if(the_path == "/content-codes")
    {
        daap_request->setRequestType(DAAP_REQUEST_CONTCODES);
        log(": a client asked for /content-codes", 9);
    }
    else if(the_path == "/login")
    {
        daap_request->setRequestType(DAAP_REQUEST_LOGIN);
        log(": a client asked for /login", 9);
    }
    else if(the_path == "/logout")
    {
        daap_request->setRequestType(DAAP_REQUEST_LOGOUT);
        log(": a client asked for /logout", 9);
    }
    else if(the_path.startsWith("/update"))
    {
        daap_request->setRequestType(DAAP_REQUEST_UPDATE);
        log(": a client asked for /update", 9);
    }
    else if(the_path.startsWith("/databases"))
    {
        daap_request->setRequestType(DAAP_REQUEST_DATABASES);
        log(QString(": a client asked for /databases (%1)").arg(the_path), 9);
    }
    else if(the_path.startsWith("/resolve"))
    {
        daap_request->setRequestType(DAAP_REQUEST_RESOLVE);
        log(": a client asked for /resolve", 9);
    }
    else if(the_path.startsWith("/browse"))
    {
        daap_request->setRequestType(DAAP_REQUEST_BROWSE);
        log(": a client asked for /browse", 9);
    }
    else
    {
        warning(QString("does not understand the path of this request: %1")
              .arg(the_path));
              
        //
        //  Send an httpd 400 error to the client
        //

        http_request->getResponse()->setError(400);

    }
    
    //
    //  Check the user agent header that httpd returns to take note of which
    //  kind of client this is (for iTunes we (will) need to re-encode some
    //  content on the fly.)
    //

    QString user_agent = http_request->getHeader("User-Agent");

    if(user_agent.contains("iTunes/4"))
    {

        daap_request->setClientType(DAAP_CLIENT_ITUNES4X);

        //
        //  Try and set a specific iTunes minor version
        //

        QString sub_version_string = user_agent.section(" ",0,0);
        sub_version_string = sub_version_string.section("/4.",1,1);
        if ( sub_version_string.contains('.') )
        {
            //
            //  For the time being, ignore the sub-sub version if there is
            //  one
            //

            sub_version_string = sub_version_string.section(".", 0, 0);
        }
        
        bool ok = true;
        int itunes_sub_version = sub_version_string.toInt(&ok);
        if(ok)
        {
            if(itunes_sub_version == 1)
            {
                daap_request->setClientType(DAAP_CLIENT_ITUNES41);
            }
            else if(itunes_sub_version == 2)
            {
                daap_request->setClientType(DAAP_CLIENT_ITUNES42);
            }
            else if(itunes_sub_version == 5)
            {
                daap_request->setClientType(DAAP_CLIENT_ITUNES45);
            }
            else if(itunes_sub_version == 6)
            {
                daap_request->setClientType(DAAP_CLIENT_ITUNES46);
            }
            else if(itunes_sub_version == 7)
            {
                daap_request->setClientType(DAAP_CLIENT_ITUNES47);
            }
            else if(itunes_sub_version == 8)
            {
                daap_request->setClientType(DAAP_CLIENT_ITUNES48);
            }
            else
            {
                warning(QString("daapserver does not yet have "
                                "specific code for this version of "
                                "iTunes: 4.%1").arg(itunes_sub_version));
            }
        }
        else
        {
            warning("could not determine iTunes minor version");
        }

    }
    else if(user_agent.contains("MythTV/1"))
    {
        daap_request->setClientType(DAAP_CLIENT_MFDDAAPCLIENT);
    }


    
}




void DaapServer::sendServerInfo(HttpInRequest *http_request, DaapRequest *daap_request)
{
    Version daapVersion3( 3, 0 );
    Version daapVersion2( 2, 0 );
    Version dmapVersion1( 1, 0 );
 
    TagOutput response;
    
    response << Tag('msrv') 
                << Tag('mstt') << (u32) DAAP_OK << end 
                << Tag('mpro') << dmapVersion1 << end;
                
                //
                //  Send a daap version number that the client _wants_ to
                //  get
                //
                
                if(daap_request->getClientType() == DAAP_CLIENT_ITUNES45 ||
                   daap_request->getClientType() == DAAP_CLIENT_ITUNES46 ||
                   daap_request->getClientType() == DAAP_CLIENT_ITUNES47 ||
                   daap_request->getClientType() == DAAP_CLIENT_ITUNES48 )
                {
                    response << Tag('apro') << daapVersion3 << end ;
                }
                else
                {
                    response << Tag('apro') << daapVersion2 << end ;
                }
                
    response    << Tag('minm') << service_name.utf8() << end 
                << Tag('mslr') << (u8) 0 << end 
                << Tag('msal') << (u8) 0 << end 
                << Tag('mstm') << (u32) 1800 << end 
                << Tag('msup') << (u8) 0 << end
                << Tag('msau') << (u8) 2 << end 
                << Tag('mspi') << (u8) 0 << end 
                << Tag('msex') << (u8) 0 << end 
                << Tag('msbr') << (u8) 0 << end 
                << Tag('msqy') << (u8) 0 << end 
                << Tag('msix') << (u8) 0 << end 
                << Tag('msrs') << (u8) 0 << end
                << Tag('msdc') << (u32) 1 << end;

             response << end;

    sendTag( http_request, response.data() );
    
}


void DaapServer::sendContentCodes(HttpInRequest *http_request)
{
    sendTag( http_request, TypeRegistry::getDictionary() );
}




void DaapServer::sendTag( HttpInRequest *http_request, const Chunk& c ) 
{
    //
    //  Note the function name is a bit of a mismomer as we are not actually
    //  sending anything here. This just loads the payload of the http/daap
    //  response with whatever data we've built up.
    //

    //
    //  Set some header stuff 
    //
    
    http_request->getResponse()->addHeader("DAAP-Server: MythTV/1.0 (Probably Linux)");
    http_request->getResponse()->addHeader("Content-Type: application/x-dmap-tagged");
    
    //
    //  Set the payload
    //
    
    http_request->getResponse()->setPayload((char*) c.begin(), c.size());
}



void DaapServer::sendLogin(HttpInRequest *http_request, u32 session_id)
{
    TagOutput response;
    response << Tag('mlog') << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('mlid') << (u32) session_id << end 
             << end;
    sendTag( http_request, response.data() );
}



void DaapServer::parseVariables(HttpInRequest *http_request, DaapRequest *daap_request)
{
    //
    //  This goes through the data that came in on the client request and
    //  assigns useful data to our DaapRequest object
    //

    bool ok;
    QString variable;


    if ( ( variable = http_request->getVariable("session-id") ) != NULL)
    {
        daap_request->setSessionId(variable.toULong(&ok));
        if(!ok)
            warning("failed to parse session id from client request");
    }

    if ( ( variable = http_request->getVariable("revision-number" ) ) != NULL)
    {
        daap_request->setDatabaseVersion(variable.toULong(&ok));
        if(!ok)
            warning("failed to parse database version from client request");
    }

    if ( ( variable = http_request->getVariable("delta" ) ) != NULL)
    {
        daap_request->setDatabaseDelta(variable.toULong(&ok));
        if(!ok)
            warning("failed to parse database delta from client request");
    }

    if ( ( variable = http_request->getVariable("type" ) ) != NULL)
    {
        daap_request->setContentType(variable);
    }
    
    if ( ( variable = http_request->getVariable("meta" ) ) != NULL)
    {
        daap_request->setRawMetaContentCodes(QStringList::split(",",variable));
        daap_request->parseRawMetaContentCodes();
    }

    if ( ( variable = http_request->getVariable("filter" ) ) != NULL)
    {
        daap_request->setFilter(variable);
    }
    
    if ( ( variable = http_request->getVariable("query" ) ) != NULL)
    {
        daap_request->setQuery(variable);
    }
    
    if ( ( variable = http_request->getVariable("index" ) ) != NULL)
    {
        daap_request->setIndex(variable);
    }

}



void DaapServer::sendUpdate(HttpInRequest *http_request, u32 database_version)
{

    TagOutput response;

    response << Tag( 'mupd' ) 
                << Tag('mstt') << (u32) DAAP_OK << end 
                << Tag('musr') << (u32) database_version << end 
             << end;

    sendTag( http_request, response.data() );
    
}


void DaapServer::sendMetadata(HttpInRequest *http_request, QString request_path, DaapRequest *daap_request)
{
    //
    //  The request_path is the key here. The client specifies what is
    //  wants, starting with a path of "/databases". We tell it how many
    //  databases there are, then it recursively requests the items and
    //  containers inside of each.
    //
    
    //
    //  if, however, the requesting client is iTunes 4.5 (or higher?), we
    //  have to munge the path a bit
    //

    if(request_path.contains("daap://"))
    {
        request_path = request_path.section(":", 2, -1);
        request_path = request_path.section("/", 1, -1);
    }
    
    QStringList components = QStringList::split("/", request_path);
    
    if(components.count() < 1)
    {
        warning("got a client request that made no sense");
        return;
    }
    
    if ( components.count() == 1 )
    {
        //
        //  It must (*must*) have asked for /databases
        //
        
        sendDatabaseList(http_request);
    } 
    else 
    {
        //
        //  Ok, it's asking for something below /databases/...
        //

        
        if( components[2] == "items" )
        {
            if( components.count() == 3 )
            {
                sendDatabase( http_request, daap_request, components[1].toInt());
            } 
            else if ( components.count() == 4 )
            {
                QString reference_to_song = components[3];
                QString cut_down = reference_to_song.section('.',-2, -2);
                bool ok;
                u32 song_id = cut_down.toULong(&ok);
                if(ok)
                {
                    sendDatabaseItem(http_request, song_id, daap_request);
                }
                else
                {
                    cerr << "Puked on this as a reference: " << components[3] << endl;
                }
            }
        }
        else if( components[2] == "containers" )
        {
            if( components.count() == 3 )
            {
                sendContainers( http_request, daap_request, components[1].toInt());
            } 
            else if ( components.count() == 5 && components[4] == "items" )
            {
                u32 container_id = components[3].toULong();
                sendContainer(http_request, container_id, components[1].toInt());

            } 
            else 
            {
                http_request->getResponse()->setError(403);
                return;
            }
        } 
        else 
        {
            warning(QString("this request makes no sense: %1")
                    .arg(request_path));
            http_request->getResponse()->setError(403);
            return;
        }
    }    
}

void DaapServer::sendDatabaseList(HttpInRequest *http_request)
{

    //
    //  Describe our only "virtual" database and how many items (songs) and
    //  containers (playlists) it has
    //

    TagOutput response;
    
    response << Tag('avdb') 
                << Tag('mstt') << (u32) DAAP_OK << end 
                << Tag('muty') << (u8) 0 << end 
                << Tag('mtco') << (u32) 1 << end 
                << Tag('mrco') << (u32) 1 << end 
                << Tag('mlcl')
                    << Tag('mlit')
                        << Tag('miid') << (u32) 1 << end 
                        //<< Tag('mper') << (u64) 543675286654 << end 
                        << Tag('minm') << service_name.utf8() << end 
                        << Tag('mimc') << (u32) metadata_server->getAllLocalAudioMetadataCount() << end 
                        << Tag('mctc') << (u32) metadata_server->getAllLocalAudioPlaylistCount() << end 
                    << end
                << end 
            << end;

    sendTag( http_request, response.data() );

}

void DaapServer::addItemToResponse(
                                    HttpInRequest *http_request,
                                    DaapRequest *daap_request, 
                                    TagOutput &response, 
                                    AudioMetadata *which_item, 
                                    u64 meta_codes
                                  )
{
    response << Tag('mlit');
                    
    //
    //  As per what we know of the "standard", 
    //  item kind, item data kind (2 = file, 1 = stream?),
    //  item id, and item name must _always_ be sent
    //
    
    response << Tag('mikd') << (u8) 2 << end;
    response << Tag('asdk') << (u8) 0 << end;
    response << Tag('miid') << (u32) which_item->getUniversalId() << end;
    response << Tag('minm') << which_item->getTitle().utf8() << end;

    //
    //  If the client is another mfd, send the mythdigest of this track
    //

    if(daap_request->getClientType() == DAAP_CLIENT_MFDDAAPCLIENT)
    {
        response << Tag('mypi') << which_item->getMythDigest().utf8() << end;
    }

    //
    //  Everything else is optional depending on
    //  what the client passed in its meta=foo
    //  GET variable
    //

    if(meta_codes & DAAP_META_PERSISTENTID)
    {
        if(which_item->getDbId() > 0)
        {
            response << Tag('mper') << (u64) which_item->getDbId() << end;
        }
    }
                    
    if(meta_codes & DAAP_META_SONGALBUM)
    {
        response << Tag('asal') << which_item->getAlbum().utf8() << end;
    }
    
    if(meta_codes & DAAP_META_SONGARTIST)
    {
        response << Tag('asar') << which_item->getArtist().utf8() << end;
    }

    if(meta_codes & DAAP_META_SONGBITRATE)
    {
        if(which_item->getBitrate() > 0)
        {
            response << Tag('asbr') << (u16) which_item->getBitrate() << end;
        }
    }

    if(meta_codes & DAAP_META_SONGBEATSPERMINUTE)
    {
        if(which_item->getBpm() > 0)
        {
            response << Tag('asbt') << (u16) which_item->getBpm() << end;
        }
    }
    
    if(meta_codes & DAAP_META_SONGCOMMENT)
    {
        if(which_item->getComment().length() > 0)
        {
            response << Tag('ascm') << which_item->getComment().utf8() << end;
        }
    }

    if(meta_codes & DAAP_META_SONGCOMPILATION)
    {
        response << Tag('asco') << (u8) which_item->getCompilation() << end;
    }
    
    if(meta_codes & DAAP_META_SONGCOMPOSER)
    {
        if(which_item->getComposer().length() > 0)
        {
            response << Tag('ascp') << which_item->getComposer().utf8() << end;
        }
    }

    if(meta_codes & DAAP_META_SONGDATEADDED)
    {
        QDateTime when = which_item->getDateAdded();
        QDateTime first_possible;
        first_possible.setTime_t(0);
        if(when > first_possible)
        {
            response << Tag('asda') << (u32) when.toTime_t() << end;
        }
    }

    if(meta_codes & DAAP_META_SONGDATEMODIFIED)
    {
        QDateTime when = which_item->getDateModified();
        QDateTime first_possible;
        first_possible.setTime_t(0);
        if(when > first_possible)
        {
            response << Tag('asdm') << (u32) when.toTime_t() << end;
        }
    }

    if(meta_codes & DAAP_META_SONGDISCCOUNT)
    {
        if(which_item->getDiscCount() > 0)
        {
            response << Tag('asdc') << (u16) which_item->getDiscCount() << end;
        }
    }
    
    if(meta_codes & DAAP_META_SONGDISCNUMBER)
    {
        if(which_item->getDiscNumber() > 0)
        {
            response << Tag('asdn') << (u16) which_item->getDiscNumber() << end;
        }
    }
    
    if(meta_codes & DAAP_META_SONGDISABLED)
    {
        response << Tag('asdb') << (u8) 0 << end;
    }
    if(meta_codes & DAAP_META_SONGEQPRESET)
    {
        if(which_item->getEqPreset().length() > 0)
        {
            response << Tag('aseq') << which_item->getEqPreset().utf8() << end;
        }
    }

    if(meta_codes & DAAP_META_SONGFORMAT)
    {
        //
        //  If the client is another Myth box, tell them the truth about
        //  most of our file formats (except wma and cda). If it's something
        //  else (e.g. iTunes) then trick them.
        //
        
        if(daap_request->getClientType() == DAAP_CLIENT_MFDDAAPCLIENT)
        {
            //
            //  If the file is a wma, stream it as a wav (because the audio
            //  plugin does not know how to handle non-file based wma
            //  content). Similarily, we always stream local cd audio tracks
            //  as wav files.
            //
            
            if(
                which_item->getFormat() == "wma" ||
                which_item->getFormat() == "cda"
              )
            {
                response << Tag('asfm') << "wav" << end;
            }
            else if(which_item->getFormat() == "m4a")
            {
                //
                //  Send aac (.m4a) as aac if the client supports it, otherwise wav
                //

                if(http_request->getHeader("Accept").contains("audio/m4a"))
                {
                    response << Tag('asfm') << "m4a" << end;
                }
                else
                {
                    response << Tag('asfm') << "wav" << end;
                }
            }
            else
            {
                QString extension = which_item->getFormat();
                response << Tag('asfm') << extension.utf8() << end;
            }
        }
        else
        {
            if(which_item->getFormat() == "mp3")
            {
                response << Tag('asfm') << "mp3" << end;
            }
            else if(which_item->getFormat() == "m4a")
            {
                response << Tag('asfm') << "m4a" << end;
            }
            else
            {
                response << Tag('asfm') << "wav" << end;
            }
        }
    }

    
    if(meta_codes & DAAP_META_SONGGENRE)
    {
        if(which_item->getGenre().length() > 0)
        {
            response << Tag('asgn') << which_item->getGenre().utf8() << end;
        }
    }   
    
    if(meta_codes & DAAP_META_SONGDESCRIPTION)
    {
        if(which_item->getDescription().length() > 0)
        {
            response << Tag('asdt') << which_item->getDescription().utf8() << end;
        }
    }
    
    if(meta_codes & DAAP_META_SONGRELATIVEVOLUME)
    {
        if(which_item->getRelativeVolume() != 0)
        {
            response << Tag('asrv') << (u8) which_item->getRelativeVolume() << end;
        }
    }
    
    if(meta_codes & DAAP_META_SONGSAMPLERATE)
    {
        if(which_item->getSampleRate() > 0)
        {
            response << Tag('assr') << (u32) which_item->getSampleRate() << end;
        }
        else
        {
            response << Tag('assr') << (u32) 0 << end;
        }
    }
    
    if(meta_codes & DAAP_META_SONGSIZE)
    {
        if(which_item->getSize() > 0)
        {
            response << Tag('assz') << (u32) which_item->getSize() << end;
        }
    }
    
    if(meta_codes & DAAP_META_SONGSTARTTIME)
    {
        if(which_item->getStartTime() > -1)
        {
            response << Tag('asst') << (u32) which_item->getStartTime() << end;
        }
    }

    if(meta_codes & DAAP_META_SONGSTOPTIME)
    {
        if(which_item->getStopTime() > -1)
        {
            response << Tag('assp') << (u32) which_item->getStopTime() << end;
        }
    }
    
    if(meta_codes & DAAP_META_SONGTIME)
    {
        response << Tag('astm') << (u32) which_item->getLength() << end;
    }
    
    if(meta_codes & DAAP_META_SONGTRACKCOUNT)
    {
        if(which_item->getTrackCount() > 0)
        {
            response << Tag('astc') << (u16) which_item->getTrackCount() << end;
        }
    }

    if(meta_codes & DAAP_META_SONGTRACKNUMBER)
    {
        response << Tag('astn') << (u16) which_item->getTrack() << end;
    }
    
    if(meta_codes & DAAP_META_SONGUSERRATING)
    {
        response << Tag('asur') << (u8) (which_item->getRating() * 10) << end;
    }

    if(meta_codes & DAAP_META_SONGYEAR)
    {
        response << Tag('asyr') << (u16) which_item->getYear() << end;
    }

    if(meta_codes & DAAP_META_SONGDATAURL)
    {
        // response << Tag('asul') << utf8 .... 
    }
    
    if(meta_codes & DAAP_META_NORMVOLUME)
    {
        // I have no idea why anyone would ever want to use this ?
    }
    
    
    response << end;
    
}

void DaapServer::sendDatabase(HttpInRequest *http_request, DaapRequest *daap_request, int which_database)
{ 
    if(which_database != 1)
    {
        warning("some client is asking about a database other than 1");
    }

    //
    //  send deltas (+/-) additions if it makes sense, otherwise send everything
    //

    uint audio_generation = metadata_server->getMetadataLocalAudioGeneration();
    uint client_db_generation = daap_request->getDatabaseVersion();
    uint client_db_delta = daap_request->getDatabaseDelta();

    metadata_changed_mutex.lock();
        int which_collection_id = metadata_collection_last_changed;
    metadata_changed_mutex.unlock();

    bool do_delta = false;
    MetadataContainer *last_changed_collection = NULL;
    if(which_collection_id == metadata_server->getLastDestroyedCollection())
    {
        do_delta = true;
    }
    else
    {
        last_changed_collection = metadata_server->getMetadataContainer(which_collection_id);
        if(last_changed_collection)
        {
            if(last_changed_collection->isAudio() && last_changed_collection->isLocal())
            {
                do_delta = true;
            }
        }
    }

    u64 meta_codes = daap_request->getParsedMetaContentCodes();
    uint audio_count = metadata_server->getAllLocalAudioMetadataCount();

    if(
       do_delta                                     &&
       client_db_delta  != 0                        &&
       client_db_generation == audio_generation     &&
       client_db_generation == client_db_delta + 1
      )
    {
        log(": sending a partial metadata update", 8);

        //
        //  The client is only off by one, so we can be slightly efficient here and only send the deltas
        //
        
        QValueList<int> additions;
        QValueList<int> deletions;

        if(which_collection_id == metadata_server->getLastDestroyedCollection())
        {
            deletions = metadata_server->getLastDestroyedMetadataList();
        }
        else
        {
            additions = last_changed_collection->getMetadataAdditions();
            deletions = last_changed_collection->getMetadataDeletions();
        }

        int additions_count = additions.count();

        TagOutput response;
        response    << Tag( 'adbs' )
                         << Tag('mstt') << (u32) DAAP_OK << end
                         << Tag('muty') << (u8) 1 << end
                         << Tag('mtco') << (u32) audio_count << end
                         << Tag('mrco') << (u32) additions_count << end;

        //
        //  Send new items (delta +)
        //
        
        if(additions_count > 0)
        {
             response << Tag('mlcl') ;
             for(uint i = 0; i < additions.count(); i++)
             {
                Metadata *added_metadata = last_changed_collection->getMetadata((*additions.at(i)));
                if(added_metadata->getType() == MDT_audio)
                {
                    AudioMetadata *added_audio_metadata = (AudioMetadata*)added_metadata;
                    addItemToResponse(
                                        http_request,
                                        daap_request, 
                                        response, 
                                        added_audio_metadata, 
                                        meta_codes
                                     );
                }
                else
                {
                    warning("getting non audio metadata off a last changed collection ... disaster");
                }
            }
            response << end;
        }
        
        
        //
        //  Send deleted items (delta -)
        //
        
        if(deletions.count() > 0)
        {
            response << Tag('mudl') ;
            for(uint i = 0; i < deletions.count(); i++)
            {
                response << Tag('miid') << (u32) ((*deletions.at(i)) + (which_collection_id * METADATA_UNIVERSAL_ID_DIVIDER )) << end;
            }
            response << end;
        }

        //
        //  Close out the adbs tag
        //
        
        response << end;
        sendTag( http_request, response.data() );
    }
    else
    {    
        log(": sending a full metadata update", 8);
        TagOutput response;
        response << Tag( 'adbs' )
                    << Tag('mstt') << (u32) DAAP_OK << end 
                    << Tag('muty') << (u8) 0 << end 
                    << Tag('mtco') << (u32) audio_count << end 
                    << Tag('mrco') << (u32) audio_count << end 
                    << Tag('mlcl') ;
             
        //
        // Iterate over all the _local_ _audio_ containers, and then over every item in
        // each container
        //
             
        MetadataContainer *a_container = NULL;
             
        for(   
            a_container = metadata_containers->first(); 
            a_container; 
            a_container = metadata_containers->next() 
           )
        {
            if(a_container->isAudio() && a_container->isLocal() )
            {
                //
                //  If the container is ripable (ie. it's an audio cd), and
                //  the client is another myth box, add a Ripable header.
                //

                if(a_container->isRipable() && daap_request->getClientType() == DAAP_CLIENT_MFDDAAPCLIENT)
                {
                    http_request->getResponse()->addHeader("Ripable: Yes");
                }

                QIntDict<Metadata>     *which_metadata;
                which_metadata = a_container->getMetadata();
                    
                if(which_metadata)
                {
                    QIntDictIterator<Metadata> iterator(*which_metadata);
                    for( ; iterator.current(); ++iterator)
                    {
                        if(iterator.current()->getType() == MDT_audio)
                        {
                            AudioMetadata *which_item = (AudioMetadata*)iterator.current();
                            addItemToResponse(
                                                http_request,
                                                daap_request, 
                                                response, 
                                                which_item, 
                                                meta_codes
                                             );
                        }
                        else
                        {
                            warning("got a metadata item off an audio collection that is not of type AudioMetadata");
                        }
                    }
                }
            }
        }
        
        response << end 
        << end;
        sendTag( http_request, response.data() );
    }
}

void DaapServer::sendDatabaseItem(HttpInRequest *http_request, u32 song_id, DaapRequest *daap_request)
{
    Metadata *which_one = metadata_server->getMetadataByUniversalId(song_id);

    if(which_one)
    {
        if(which_one->getType() == MDT_audio)
        {
            //http_request->printHeaders();   //  If you want to see the request

            //
            //  All daap responses must be dmap-tagged, even those (such as
            //  this) where we are _not_ sending and DMAP.
            //

            http_request->getResponse()->addHeader("Content-Type: application/x-dmap-tagged");
            
            //
            //  If the client is iTunes version 4.5/4.6, it needs the word
            //  "bytes" in the Content Range header
            //
            
            if(daap_request->getClientType() == DAAP_CLIENT_ITUNES45 ||
               daap_request->getClientType() == DAAP_CLIENT_ITUNES46 ||
               daap_request->getClientType() == DAAP_CLIENT_ITUNES47 ||
               daap_request->getClientType() == DAAP_CLIENT_ITUNES48)
            {
                http_request->getResponse()->setBytesInContentRangeHeader(true);
            }


            AudioMetadata *which_audio = (AudioMetadata*)which_one;
            QString file_path = which_audio->getFilePath();

            //
            //  If we got a Range: header in the request, send only the
            //  relevant part of the file.
            //

            int skip = 0;
            
            if(http_request->getHeader("Range"))
            {
                QString range_string = http_request->getHeader("Range");
                
                range_string = range_string.remove("bytes=");
                range_string = range_string.section("-",0,0);
                bool ok;
                skip = range_string.toInt(&ok);
                if(!ok)
                {
                    warning(QString("did not understand this Range request in an http request %1")
                            .arg(http_request->getHeader("Range")));
                    skip = 0;
                }
                
            }
            
            //
            //  Now, to send the actual file. If the client is not of type
            //  DAAP_CLIENT_MFDDAAPCLIENT (ie. another mfd's daap client),
            //  then we need to decode some file formats on the fly before
            //  sending them.
            //
            
            if(daap_request->getClientType() == DAAP_CLIENT_MFDDAAPCLIENT)
            {
                //
                //  Always translate wma's and cda's
                //
                
                if(which_audio->getFormat() == "m4a")
                {
                    if(http_request->getHeader("Accept").contains("audio/m4a"))
                    {
                        http_request->getResponse()->sendFile(file_path, skip);
                    }
                    else
                    {
                        http_request->getResponse()->sendFile(file_path, skip, FILE_TRANSFORM_TOWAV);
                    }
                }
                else if(
                    which_audio->getFormat() == "wma" ||
                    which_audio->getFormat() == "cda"
                  )
                {
                    http_request->getResponse()->sendFile(file_path, skip, FILE_TRANSFORM_TOWAV);
                }
                else
                {
                    http_request->getResponse()->sendFile(file_path, skip);
                }
            }
            else
            {
                //
                //  The client is iTunes or an iTunes clone, translate non
                //  mp3, ac3, etc. to wav format
                //
                
                if(
                    which_audio->getFormat() == "mp3" ||
                    which_audio->getFormat() == "m4a"
                  )
                {
                    http_request->getResponse()->sendFile(file_path, skip);
                }
                else
                {
                    http_request->getResponse()->sendFile(file_path, skip, FILE_TRANSFORM_TOWAV);
                }

            }


        }
        else
        {
            warning("got a reference to a non audio bit of metadata");
        }
    }
    else
    {
        warning("got a bad reference to a metadata item");
    }

}

void DaapServer::sendContainers(HttpInRequest *http_request, DaapRequest *daap_request, int which_database)
{
    if(which_database != 1)
    {
        warning("some client asked for containers in a database other than 1");
    }
    

    uint all_metadata_count = metadata_server->getAllLocalAudioMetadataCount();
    uint all_playlist_count = metadata_server->getAllLocalAudioPlaylistCount();
    
    //
    //  if the client is not another mfd, we have to add a fake playlist with all the items
    //

    if(daap_request->getClientType() != DAAP_CLIENT_MFDDAAPCLIENT)
    {
        ++all_playlist_count;
    }
    
    TagOutput response;
    response << Tag( 'aply' ) 
                << Tag('mstt') << (u32) DAAP_OK << end 
                << Tag('muty') << (u8) 0 << end 
                << Tag('mtco') << (u32) all_playlist_count << end
                << Tag('mrco') << (u32) all_playlist_count << end 
                << Tag('mlcl');
             
                
                //
                //  If non-mfd, send playlist "1" as all the metadata
                //
             
                if(daap_request->getClientType() != DAAP_CLIENT_MFDDAAPCLIENT)
                {
                    response << Tag('mlit');
                        response << Tag('miid') << (u32) 1 << end ;
                        response << Tag('minm') << service_name.utf8() << end ;
                        response << Tag('mimc') << (u32) all_metadata_count << end ;
                    response << end;
                }
             
                //
                // For each container, explain it
                //
             
                MetadataContainer *a_container = NULL;
                for(   
                    a_container = metadata_containers->first(); 
                    a_container; 
                    a_container = metadata_containers->next() 
                   )
                {
                    if(a_container->isAudio() && a_container->isLocal() )
                    {
                        QIntDict<Playlist> *which_playlists = a_container->getPlaylists();
                        if(which_playlists)
                        {
                            QIntDictIterator<Playlist> it( *which_playlists );

                            Playlist *a_playlist;
                            while ( (a_playlist = it.current()) != 0 )
                            {
                                ++it;
             
                                response << Tag('mlit');
                                    response << Tag('miid') << (u32) a_playlist->getUniversalId() << end ;
                                    response << Tag('minm') << a_playlist->getName().utf8() << end ;
                                    response << Tag('mimc') << (u32) a_playlist->getFlatCount() << end ;
                                response << end;
                            }
                        }
                    }
                }
                response << end;
                
                //
                //  Send any deletions (playlists gone away) if we have any
                //  and this is a proper partial (delta) update
                //


                if(daap_request->getDatabaseDelta() != 0 &&
                   daap_request->getDatabaseVersion() == daap_request->getDatabaseDelta() + 1 &&
                   metadata_server->getMetadataLocalAudioGeneration() == daap_request->getDatabaseVersion())
                { 
                   
                    bool do_deletions = false;

                    metadata_changed_mutex.lock();
                        int which_collection_id = metadata_collection_last_changed;
                    metadata_changed_mutex.unlock();
                    MetadataContainer *last_changed_collection = NULL;
                    if(which_collection_id == metadata_server->getLastDestroyedCollection())
                    {
                        do_deletions = true;
                    }
                    else
                    {
                        last_changed_collection = metadata_server->getMetadataContainer(which_collection_id);
                        if(last_changed_collection)
                        {
                            if(last_changed_collection->isAudio() && last_changed_collection->isLocal())
                            {
                                do_deletions = true;
                                which_collection_id = last_changed_collection->getIdentifier();
                            }
                        }
                    }
                    if(do_deletions)
                    {
                
                        QValueList<int> deletions;
                        if(last_changed_collection)
                        {
                            deletions = last_changed_collection->getPlaylistDeletions();
                        }
                        else
                        {
                            deletions = metadata_server->getLastDestroyedPlaylistList();
                        }
                    
                        if(deletions.count() > 0)
                        {
                            response << Tag('mudl');
                            for(uint i = 0; i < deletions.count(); i++)
                            {
                                int deletion_value = *deletions.at(i);
                                deletion_value = (which_collection_id * METADATA_UNIVERSAL_ID_DIVIDER) + deletion_value;
                                response << Tag('miid') << (u32) deletion_value << end;
                            }
                            response << end;
                        }
                    }
                }
                
             
        response << end;
    sendTag( http_request, response.data() );
}

void DaapServer::sendContainer(HttpInRequest *http_request, u32 container_id, int which_database)
{
    if(which_database != 1)
    {
        warning("some client wanted details about a container which is not in database 1");
    }
    
    //
    //  Get the playlist in question, unless it's out special "all metadata" playlist (id = 1)
    //

    uint how_many;

    Playlist *the_playlist = NULL;
    
    if(container_id != 1)
    {
        the_playlist = metadata_server->getPlaylistByUniversalId(container_id);
        if(!the_playlist)
        {
            warning("asked for a playlist it can't find");
            http_request->getResponse()->setError(404);
            return;
        }
        else
        {
            how_many = the_playlist->getFlatCount();
        }
    }
    else
    {
        how_many = metadata_server->getAllLocalAudioMetadataCount();
    }
    
    TagOutput response;

    response << Tag( 'apso' ) 
                << Tag('mstt') << (u32) DAAP_OK << end 
                << Tag('muty') << (u8) 0 << end 
                << Tag('mtco') << (u32) how_many << end 
                << Tag('mrco') << (u32) how_many << end 

                << Tag('mlcl');
                
                    //
                    //  For everthing inside this container
                    //

                    if(container_id != 1)
                    {
                    
                        QValueList<int> songs_in_list = the_playlist->getList();
                        QValueList<int>::iterator iter;
                        for ( iter = songs_in_list.begin(); iter != songs_in_list.end(); ++iter )
                        {
                            if((*iter) > 0)
                            {
                                int actual_id = (*iter) + (METADATA_UNIVERSAL_ID_DIVIDER * the_playlist->getCollectionId());
                                response << Tag('mlit') 
                                            << Tag('mikd') << (u8) 2  << end
                                            << Tag('miid') << (u32) actual_id << end 
                                            << Tag('mcti') << (u32) actual_id << end 
                                         << end;
                            }
                            else
                            {
                                int sub_playlist_id = (*iter);
                                sub_playlist_id = sub_playlist_id * -1;
                                addPlaylistWithinPlaylist(response, http_request, sub_playlist_id, the_playlist->getCollectionId());
                            }
                        }
                    }
                    else
                    {
                    
                        //
                        //  If it's playlist #1, that's *all* the metadata
                        //

                        MetadataContainer *a_container = NULL;
                        for(   
                            a_container = metadata_containers->first(); 
                            a_container; 
                            a_container = metadata_containers->next() 
                           )
                        {
                            if(a_container->isAudio() && a_container->isLocal() )
                            {
                                QIntDict<Metadata>     *which_metadata;
                                which_metadata = a_container->getMetadata();
                            
                                if(which_metadata)
                                {
                                    QIntDictIterator<Metadata> iterator(*which_metadata);
            
                                    for( ; iterator.current(); ++iterator)
                                    {
                                        if(iterator.current()->getType() == MDT_audio)
                                        {
                                            AudioMetadata *which_item = (AudioMetadata*)iterator.current();
                                            response << Tag('mlit') 
                                                        << Tag('mikd') << (u8) 2  << end
                                                        << Tag('miid') << (u32) which_item->getUniversalId() << end 
                                                        << Tag('mcti') << (u32) which_item->getUniversalId() << end 
                                                     << end;
                                        }
                                        else
                                        {
                                            warning("got non AudioMetadata of an audio metadata list");
                                        }
                                    }
                                }
                            }
                        }
                    }

                response << end 
    << end;

    sendTag( http_request, response.data() );
}

void DaapServer::addPlaylistWithinPlaylist(
                                            TagOutput &response, 
                                            HttpInRequest *http_request, 
                                            int which_playlist, 
                                            int which_container
                                          )
{
    Playlist *the_playlist = NULL;
    
    the_playlist = metadata_server->getPlaylistByContainerAndId(which_container, which_playlist);
    if(!the_playlist)
    {
        warning("asked for playlist within playlist with bad reference");
        http_request->getResponse()->setError(404);
        return;
    }


    QValueList<int> songs_in_list = the_playlist->getList();
    QValueList<int>::iterator iter;
    for ( iter = songs_in_list.begin(); iter != songs_in_list.end(); ++iter )
    {
        if((*iter) > 0)
        {
            int actual_id = (*iter) + (METADATA_UNIVERSAL_ID_DIVIDER * the_playlist->getCollectionId());
            response << Tag('mlit') 
                        << Tag('mikd') << (u8) 2  << end
                        << Tag('miid') << (u32) actual_id << end 
                        << Tag('mcti') << (u32) actual_id << end 
                     << end;
        }
        else
        {
            int sub_playlist_id = (*iter);
            sub_playlist_id = sub_playlist_id * -1;
            addPlaylistWithinPlaylist(response, http_request, sub_playlist_id, the_playlist->getCollectionId());
        }
    }
}


void DaapServer::handleMetadataChange(int which_collection, bool /* external */)
{
    bool take_action = false;
    uint audio_generation = 0;
    if(which_collection > 0)
    {
        metadata_server->lockMetadata();
            MetadataContainer *which_one = metadata_server->getMetadataContainer(which_collection);
            if(which_one)
            {
                if(which_one->isAudio() && which_one->isLocal())
                {
                    take_action = true;
                }
                audio_generation = metadata_server->getMetadataLocalAudioGeneration();
            }
            else
            {
                take_action = true;
                audio_generation = metadata_server->getMetadataLocalAudioGeneration();
            } 
        metadata_server->unlockMetadata();
    }
    else
    {
        take_action = true;
        audio_generation = metadata_server->getMetadataLocalAudioGeneration();
    }

    if(take_action)
    {
        //
        //  I need to tell any client sitting in a hanging update that
        //  metadata has changed and remove them from the hanging list
        //

        TagOutput response;

        response << Tag( 'mupd' ) 
                    << Tag('mstt') << (u32) DAAP_OK << end 
                    << Tag('musr') << (u32) audio_generation << end;
        
        response << end;
        

        HttpOutResponse *hu_response = new HttpOutResponse(this, NULL);
        hu_response->addHeader("DAAP-Server: MythTV/1.0 (Probably Linux)");
        hu_response->addHeader("Content-Type: application/x-dmap-tagged");
        Chunk c = response.data();
        hu_response->setPayload((char*) c.begin(), c.size());

        
        hanging_updates_mutex.lock();
            QValueList<int>::iterator it;
            for ( it = hanging_updates.begin(); it != hanging_updates.end(); ++it )
            {
                sendResponse((*it), hu_response);
            }
            hanging_updates.clear();
        hanging_updates_mutex.unlock();
        
        delete hu_response;
    }
}

DaapServer::~DaapServer()
{
    //
    //  Tell any clients that are hanging on an /update that we're going
    //  away
    //
    
    //
    //  Not really sure what iTunes says when it is going away, we send a 404
    //  FIX when daapclient is working well enough that we can find out what
    //  iTunes sends
    //
        
    HttpOutResponse *going_away_response = new HttpOutResponse(this, NULL);
    going_away_response->setError(404);

    hanging_updates_mutex.lock();
        QValueList<int>::iterator it;
        for(it = hanging_updates.begin(); it != hanging_updates.end(); ++it ) 
        {
            sendResponse((*it), going_away_response);
        }
    hanging_updates_mutex.unlock();
    
    delete going_away_response;    
}
