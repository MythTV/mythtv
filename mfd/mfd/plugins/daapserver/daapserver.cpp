/*
    daapserver.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    

*/

#include <qapplication.h>
#include <qvaluelist.h>

#include "mfd_events.h"
#include "httprequest.h"
#include "httpresponse.h"

#include "daapserver.h"
#include "mcc_bitfield.h"


#include "daaplib/registry.h"

DaapServer *daap_server = NULL;


const int   DAAP_OK = 200;


DaapServer::DaapServer(MFD *owner, int identity)
      :MFDHttpPlugin(owner, identity, 3689, "daap server", 2)
{
    first_update = true;
    metadata_server = owner->getMetadataServer();
    metadata_containers = metadata_server->getMetadataContainers();    
    QString local_hostname = "unknown";
    char my_hostname[2049];
    if(gethostname(my_hostname, 2048) < 0)
    {
        warning("could not call gethostname()");
        return;
    }
    else
    {
        local_hostname = my_hostname;
    }
    service_name = QString("MythMusic (DAAP) on %1").arg(local_hostname); 

    //
    //  Register this (daap) service
    //

    ServiceEvent *se = new ServiceEvent(QString("services add daap %1 %2")
                                       .arg(port_number)
                                       .arg(service_name));
    QApplication::postEvent(parent, se);


}


void DaapServer::handleIncoming(HttpRequest *http_request, int client_id)
{
    //
    //  Create a DaapRequest object that will be built up to understand the
    //  request that has just come in from a client
    //

    DaapRequest *daap_request = new DaapRequest();

    //
    //  Start to build up the request by parsing the path of the request.
    //
    
    parsePath(http_request, daap_request);
    
    //
    //  Make sure we understand the path
    //

    if(daap_request->getRequestType()       == DAAP_REQUEST_NOREQUEST)
    {
        return;
    }
    
    //
    //  Send back whatever the client asked for, based on the request type
    //
    
    else if( daap_request->getRequestType() == DAAP_REQUEST_SERVINFO )
    {
        sendServerInfo(http_request);
        return;
    }   

    else if( daap_request->getRequestType() == DAAP_REQUEST_LOGIN)
    {
        u32 session_id = daap_sessions.getNewId();
        sendLogin( http_request, session_id);
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

            uint audio_generation = metadata_server->getMetadataAudioGeneration();

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
        //  Log some data about Client-DAAP-Validation
        //
        
        if(
            http_request->getHeader("Client-DAAP-Validation") &&
            daap_request->getClientType() == DAAP_CLIENT_ITUNES4X
          )
        {
            if(first_update)    // Do I need to mutex this test?    
            {
                first_update_mutex.lock();
                    first_update = false;
                first_update_mutex.unlock();
                log(QString("map the md5 checksum relationship between \"%1\" "
                            "and \"%2\", win a prize")
                            .arg(http_request->getRequest())
                            .arg(http_request->getHeader("Client-DAAP-Validation"))
                            ,10);
            }
        }
    
        //
        //  
        //  should only respond if the database version numbers are out of
        //  whack.
        //


        uint audio_generation = metadata_server->getMetadataAudioGeneration();
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
    
}


void DaapServer::parsePath(HttpRequest *http_request, DaapRequest *daap_request)
{
         
    QString the_path = http_request->getUrl();

    //
    //  Figure out what kind of request this is
    //
    
    if(the_path == "/server-info")
    {
        daap_request->setRequestType(DAAP_REQUEST_SERVINFO);
        log(": a client asked for /server-info", 9);
    }
    else if(the_path == "/content_codes")
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
}




void DaapServer::sendServerInfo(HttpRequest *http_request)
{
    Version daapVersion( 2, 0 );
    Version dmapVersion( 1, 0 );
 
    TagOutput response;
    
    response << Tag('msrv') 
                << Tag('mstt') << (u32) DAAP_OK << end 
                << Tag('mpro') << dmapVersion << end 
                << Tag('apro') << daapVersion << end 
                << Tag('minm') << service_name.utf8() << end 
                << Tag('mslr') << (u8) 0 << end 
                << Tag('msal') << (u8) 0 << end 
                << Tag('mstm') << (u32) 1800 << end 
                << Tag('msup') << (u8) 0 << end 
                << Tag('msau') << (u32) 2 << end 

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




void DaapServer::sendTag( HttpRequest *http_request, const Chunk& c ) 
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



void DaapServer::sendLogin(HttpRequest *http_request, u32 session_id)
{
    TagOutput response;
    response << Tag('mlog') << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('mlid') << (u32) session_id << end 
             << end;
    sendTag( http_request, response.data() );
}



void DaapServer::parseVariables(HttpRequest *http_request, DaapRequest *daap_request)
{
    //
    //  Check the user agent header that httpd returns to take note of which
    //  kind of client this is (for iTunes we (will) need to re-encode some
    //  content on the fly.)
    //

    QString user_agent = http_request->getHeader("User-Agent");

    if(user_agent.contains("iTunes/4"))
    {
        daap_request->setClientType(DAAP_CLIENT_ITUNES4X);
    }
    else if(user_agent.contains("MythTV/1"))
    {
        daap_request->setClientType(DAAP_CLIENT_MFDDAAPCLIENT);
    }

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



void DaapServer::sendUpdate(HttpRequest *http_request, u32 database_version)
{

    TagOutput response;

    response << Tag( 'mupd' ) 
                << Tag('mstt') << (u32) DAAP_OK << end 
                << Tag('musr') << (u32) database_version << end 
             << end;

    sendTag( http_request, response.data() );
    
}


void DaapServer::sendMetadata(HttpRequest *http_request, QString request_path, DaapRequest *daap_request)
{
    //
    //  The request_path is the key here. The client specifies what is
    //  wants, starting with a path of "/databases". We tell it how many
    //  databases there are, then it recursively requests the items and
    //  containers inside of each.
    //
    
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

void DaapServer::sendDatabaseList(HttpRequest *http_request)
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

void DaapServer::addItemToResponse(DaapRequest *daap_request, TagOutput &response, AudioMetadata *which_item, u64 meta_codes)
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
        //  If the client is another Myth box, tell them the truth about our
        //  file formats. If it's something else (e.g. iTunes) then trick
        //  them.
        //
        
        if(daap_request->getClientType() == DAAP_CLIENT_MFDDAAPCLIENT)
        {
            QString extension = which_item->getUrl().fileName().section('.', -1,-1);
            response << Tag('asfm') << extension.utf8() << end;
        }
        else
        {
            if(which_item->getUrl().fileName().section('.', -1,-1) == "mp3")
            {
                response << Tag('asfm') << "mp3" << end;
            }
            else if(which_item->getUrl().fileName().section('.', -1,-1) == "m4a")
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
        if(which_item->getGenre().length() > 0 &&
           which_item->getGenre() != "Unknown" &&
           which_item->getGenre() != "Unknown Genre")
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

void DaapServer::sendDatabase(HttpRequest *http_request, DaapRequest *daap_request, int which_database)
{ 
    if(which_database != 1)
    {
        warning("some client is asking about a database other than 1");
    }

    //
    //  send deltas (+/-) additions if it makes sense, otherwise send everything
    //

    uint audio_generation = metadata_server->getMetadataAudioGeneration();
    uint client_db_generation = daap_request->getDatabaseVersion();
    uint client_db_delta = daap_request->getDatabaseDelta();

    metadata_changed_mutex.lock();
        int which_collection_id = metadata_collection_last_changed;
    metadata_changed_mutex.unlock();

    MetadataContainer *last_changed_collection = metadata_server->getMetadataContainer(which_collection_id);

    u64 meta_codes = daap_request->getParsedMetaContentCodes();
    uint audio_count = metadata_server->getAllLocalAudioMetadataCount();

    if(last_changed_collection != NULL              &&
       last_changed_collection->isAudio()           &&
       last_changed_collection->isLocal()           &&
       client_db_delta  != 0                        &&
       client_db_generation == audio_generation     &&
       client_db_generation == client_db_delta + 1
      )
    {

        //
        //  The client is only off by one, so we can be slightly efficient here and only send the deltas
        //
        
        QValueList<int> *additions = last_changed_collection->getMetadataAdditions();
        QValueList<int> *deletions = last_changed_collection->getMetadataDeletions();

        TagOutput response;
        response    << Tag( 'adbs' )
                         << Tag('mstt') << (u32) DAAP_OK << end
                         << Tag('muty') << (u8) 0 << end
                         << Tag('mtco') << (u32) audio_count << end
                         << Tag('mrco') << (u32) additions->count() << end;

        //
        //  Send new items (delta +)
        //
        
        if(additions->count() > 0)
        {
             response << Tag('mlcl') ;
             for(uint i = 0; i < additions->count(); i++)
             {
                Metadata *added_metadata = last_changed_collection->getMetadata((*additions->at(i)));
                if(added_metadata->getType() == MDT_audio)
                {
                    AudioMetadata *added_audio_metadata = (AudioMetadata*)added_metadata;
                    addItemToResponse(daap_request, response, added_audio_metadata, meta_codes);
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
        
        if(deletions->count() > 0)
        {
            response << Tag('mudl') ;
            for(uint i = 0; i < deletions->count(); i++)
            {
                response << Tag('miid') << (u32) ((*deletions->at(i)) + (which_collection_id * METADATA_UNIVERSAL_ID_DIVIDER )) << end;
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
        TagOutput response;
        response << Tag( 'adbs' )
                    << Tag('mstt') << (u32) DAAP_OK << end 
                    << Tag('muty') << (u8) 0 << end 
                    << Tag('mtco') << (u32) audio_count << end 
                    << Tag('mrco') << (u32) audio_count << end 
                    << Tag('mlcl') ;
             
        //
        // Iterate over all the containers, and then over every item in
        // each container
        //
             
        MetadataContainer *a_container = NULL;
             
        for(   
            a_container = metadata_containers->first(); 
            a_container; 
            a_container = metadata_containers->next() 
           )
        {
            if(a_container->isAudio())
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
                            addItemToResponse(daap_request, response, which_item, meta_codes);
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

void DaapServer::sendDatabaseItem(HttpRequest *http_request, u32 song_id, DaapRequest *daap_request)
{
    Metadata *which_one = metadata_server->getMetadataByUniversalId(song_id);

    if(which_one)
    {
        if(which_one->getType() == MDT_audio)
        {
            //
            //  Although it makes little sense to set the Content-Type to
            //  application/x-dmap-tagged when we're about to stream a file,
            //  that's what iTunes does ... so that's what we do
            //

            //  http_request->printHeaders();   //  If you want to see the request

            http_request->getResponse()->addHeader("Content-Type: application/x-dmap-tagged");

            AudioMetadata *which_audio = (AudioMetadata*)which_one;
            QUrl file_url = which_audio->getUrl();
            QString file_path = file_url.path();

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
                http_request->getResponse()->sendFile(file_path, skip);
            }
            else
            {
                //
                //  The client is iTunes or an iTunes clone, translate non
                //  mp3, ac3, etc. to wav format
                //
                
                if(
                    file_path.section('.', -1,-1) == "mp3" ||
                    file_path.section('.', -1,-1) == "mp4" ||
                    file_path.section('.', -1,-1) == "aac" ||
                    file_path.section('.', -1,-1) == "m4a"
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

void DaapServer::sendContainers(HttpRequest *http_request, DaapRequest *daap_request, int which_database)
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
                    if(a_container->isAudio())
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
                                    response << Tag('mimc') << (u32) a_playlist->getCount() << end ;
                                response << end;
                            }
                        }
                    }
                }
             
            response << end
        << end;
    sendTag( http_request, response.data() );
}

void DaapServer::sendContainer(HttpRequest *http_request, u32 container_id, int which_database)
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
            how_many = the_playlist->getCount();
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
                        QValueList<uint> songs_in_list = the_playlist->getList();
                        QValueList<uint>::iterator iter;
                        for ( iter = songs_in_list.begin(); iter != songs_in_list.end(); ++iter )
                        {
                            int actual_id = (*iter) + (METADATA_UNIVERSAL_ID_DIVIDER * the_playlist->getCollectionId());
                            response << Tag('mlit') 
                                        << Tag('mikd') << (u8) 2  << end
                                        << Tag('miid') << (u32) actual_id << end 
                                        << Tag('mcti') << (u32) actual_id << end 
                                     << end;
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
                            if(a_container->isAudio())
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

void DaapServer::handleMetadataChange(int which_collection)
{
    bool take_action = false;
    uint audio_generation = 0;
    if(which_collection > 0)
    {
        metadata_server->lockMetadata();
            MetadataContainer *which_one = metadata_server->getMetadataContainer(which_collection);
            if(which_one->isAudio() && which_one->isLocal())
            {
                take_action = true;
            }
            audio_generation = metadata_server->getMetadataAudioGeneration();
        metadata_server->unlockMetadata();
    }
    else
    {
        take_action = true;
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
        

        HttpResponse *hu_response = new HttpResponse(this, NULL);
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
        
    HttpResponse *going_away_response = new HttpResponse(this, NULL);
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
