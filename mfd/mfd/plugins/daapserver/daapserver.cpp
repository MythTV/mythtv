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


#include <iostream>
using namespace std;

#include "daaplib/tagoutput.h"
#include "daaplib/registry.h"

DaapServer *daap_server = NULL;


const int   DAAP_OK = 200;


DaapServer::DaapServer(MFD *owner, int identity)
      :MFDHttpPlugin(owner, identity, 3689)
{
    setName("daap server");
    metadata_containers = owner->getMetadataContainers();    
    QString local_hostname = "unknown";
    char my_hostname[2049];
    if(gethostname(my_hostname, 2048) < 0)
    {
        warning("daapserver could not call gethostname()");
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


void DaapServer::handleIncoming(HttpRequest *http_request)
{
    metadata_audio_generation = parent->getMetadataAudioGeneration();

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
    //  IF we've made it this far, then we have a logged in client, so we
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

        
        if(daap_request->getDatabaseVersion() != parent->getMetadataAudioGeneration() &&
           daap_request->getDatabaseVersion() != 0)
        {
            log(QString("daap client asked for a database request, "
                        "but has stale db reference "
                        "(mfd db# = %1, client db# = %2)")
                        .arg(parent->getMetadataAudioGeneration())
                        .arg(daap_request->getDatabaseVersion()), 9);

            sendUpdate(http_request, parent->getMetadataAudioGeneration());
        }
        else
        {
            sendMetadata(http_request, http_request->getUrl(), daap_request);
        }        
        return;        
    }
    else if (daap_request->getRequestType() == DAAP_REQUEST_UPDATE)
    {
        //
        //  
        //  should only respond if the database version numbers are out of
        //  whack.
        //


        if(daap_request->getDatabaseVersion() != parent->getMetadataAudioGeneration())
        {
            log(QString("daap client asked for and will get an update "
                        "(mfd db# = %1, client db# = %2)")
                        .arg(parent->getMetadataAudioGeneration())
                        .arg(daap_request->getDatabaseVersion()), 9);
            sendUpdate(http_request, parent->getMetadataAudioGeneration());
        }
        else
        {
            //
            //  We can hold this request, so that when (if?) the database of
            //  music does change, we have some way to push back the update
            //
            
            //  FIX            
            http_request->sendResponse(false);
        }
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
        log("daap client asked for /server-info", 9);
    }
    else if(the_path == "/content_codes")
    {
        daap_request->setRequestType(DAAP_REQUEST_CONTCODES);
        log("daap client asked for /content-codes", 9);
    }
    else if(the_path == "/login")
    {
        daap_request->setRequestType(DAAP_REQUEST_LOGIN);
        log("daap client asked for /login", 9);
    }
    else if(the_path == "/logout")
    {
        daap_request->setRequestType(DAAP_REQUEST_LOGOUT);
        log("daap client asked for /logout", 9);
    }
    else if(the_path.startsWith("/update"))
    {
        daap_request->setRequestType(DAAP_REQUEST_UPDATE);
        log("daap client asked for /update", 9);
    }
    else if(the_path.startsWith("/databases"))
    {
        daap_request->setRequestType(DAAP_REQUEST_DATABASES);
        log(QString("daap client asked for /databases (%1)").arg(the_path), 9);
    }
    else if(the_path.startsWith("/resolve"))
    {
        daap_request->setRequestType(DAAP_REQUEST_RESOLVE);
        log("daap client asked for /resolve", 9);
    }
    else if(the_path.startsWith("/browse"))
    {
        daap_request->setRequestType(DAAP_REQUEST_BROWSE);
        log("daap client asked for /browse", 9);
    }
    else
    {
        warning(QString("daapserver does not understand the path of this request: %1")
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
 
/*
    double version = httpdRequestDaapVersion( server );

    if( version == 1.0 ) {
        daapVersion.hi = 1;
        daapVersion.lo = 0;
    } 
*/
    
    TagOutput response;
    
    response << Tag('msrv') << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('mpro') << dmapVersion << end 
             << Tag('apro') << daapVersion << end 
             << Tag('minm') << service_name.utf8() << end 
             << Tag('mslr') << (u8) 0 << end 
             << Tag('msal') << (u8) 0 << end 
             << Tag('mstm') << (u32) 1800 << end 
             << Tag('msup') << (u8) 0 << end 
             << Tag('msau') << (u32) 2 << end 

             //<< Tag('mspi') << (u8) 1 << end 
             //<< Tag('msex') << (u8) 1 << end 
             //<< Tag('msbr') << (u8) 1 << end 
             //<< Tag('msqy') << (u8) 1 << end 
             //<< Tag('msix') << (u8) 1 << end 
             //<< Tag('msrs') << (u8) 1 << end 

             << Tag('msdc') << (u32) 1 << end   //  if only iTunes *did something* with more than 1 database
             
             << end;

    sendTag( http_request, response.data() );
    
}




void DaapServer::sendTag( HttpRequest *http_request, const Chunk& c ) 
{
    //
    //  Set some header stuff 
    //
    
    http_request->getResponse()->addHeader("Content-Type: application/x-dmap-tagged");
    http_request->getResponse()->addHeader(QString("Content-Length: %1")
                                           .arg(c.size()));
    //
    //  Set the payload
    //
    
    http_request->getResponse()->setPayload((char*) c.begin(), c.size());
    
    //
    //  We are done ... this will exit out and the base class will send it off
    //
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
    //  content on the fly.
    //

    QString user_agent = http_request->getHeader("User-Agent");
    // QString user_agent = server->request.userAgent;
    if(user_agent.contains("iTunes/4"))
    {
        daap_request->setClientType(DAAP_CLIENT_ITUNES4X);
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
            warning("daapserver failed to parse session id from client request");
    }

    if ( ( variable = http_request->getVariable("revision-number" ) ) != NULL)
    {
        daap_request->setDatabaseVersion(variable.toULong(&ok));
        if(!ok)
            warning("daapserver failed to parse database version from client request");
    }

    if ( ( variable = http_request->getVariable("delta" ) ) != NULL)
    {
        daap_request->setDatabaseDelta(variable.toULong(&ok));
        if(!ok)
            warning("daapserver failed to parse database delta from client request");
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

    response << Tag( 'mupd' ) << Tag('mstt') << (u32) DAAP_OK << end 
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
        warning("daapserver got a client request that made no sense");
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

        
        /*
        if( components[1].toULong() != (ulong) parent->getMetadataAudioGeneration() ||
            components.count() < 3)
        {
            //
            //  I don't know what it's asking for ... doesn't make sense ... or db is out of date
            //
            
            http_request->getResponse()->setError(403);
            return;
        }
        */
 
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
                    sendDatabaseItem(http_request, song_id);
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
            http_request->getResponse()->setError(403);
            return;
        }
    }    
}

void DaapServer::sendDatabaseList(HttpRequest *http_request)
{

    //
    //  Since iTunes (and, therefore, the notion of a "reference" daap
    //  client) is somewhat brain dead, we have to fold all our *audio*
    //  collections together to make them look like a single collection
    //
    //  So, as far as iTunes is concerned, there is only 1 database which
    //  contains *all* audio items and containers/playlists.
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
                        << Tag('mimc') << (u32) parent->getAllAudioMetadataCount() << end 
             
                        << Tag('mctc') << (u32) parent->getAllAudioPlaylistCount() << end 
                    << end
                << end 
             << end;

    sendTag( http_request, response.data() );

}

void DaapServer::sendDatabase(HttpRequest *http_request, DaapRequest *daap_request, int which_database)
{ 
    if(which_database != 1)
    {
        warning("daap server was asked about a database other than 1");
        return;
    }

    TagOutput response;
    response << Tag( 'adbs' ) << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('muty') << (u8) 0 << end 
             << Tag('mtco') << (u32) parent->getAllAudioMetadataCount() << end 
             << Tag('mrco') << (u32) parent->getAllAudioPlaylistCount() << end 
             << Tag('mlcl') ;
             
             //
             // Lock all the metadata before we do this
             //
             
             parent->lockMetadata();
             
             //
             // Iterate over all the containers, and then over every item in
             // each container
             //
             
             u64 meta_codes = daap_request->getParsedMetaContentCodes();
             MetadataContainer *a_container = NULL;
             
             for (   
                    a_container = metadata_containers->first(); 
                    a_container; 
                    a_container = metadata_containers->next() 
                 )
             {
                if(a_container->isAudio())
                {
             
                    QIntDict<Metadata>     *which_metadata;
                    which_metadata = a_container->getMetadata();

                    QIntDictIterator<Metadata> iterator(*which_metadata);
            
                    for( ; iterator.current(); ++iterator)
                    {
                        if(iterator.current()->getType() == MDT_audio)
                        {
                            AudioMetadata *which_item = (AudioMetadata*)iterator.current();

                            response << Tag('mlit');
                    
                            if(meta_codes & DAAP_META_ITEMKIND)
                            {
                                response << Tag('mikd') << (u8) 2 << end;
                            }

                            if(meta_codes & DAAP_META_SONGDATAKIND)
                            {
                                //
                                //  NB ... I think 1 is an internet radio stream
                                //
                                response << Tag('asdk') << (u8) 0 << end;
                            }
                    
                            if(meta_codes & DAAP_META_ITEMNAME)
                            {
                                response << Tag('minm') << which_item->getTitle().utf8() << end;
                            }

                            if(meta_codes & DAAP_META_ITEMID)
                            {
                                response << Tag('miid') << (u32) which_item->getId() << end;
                            }
                    
                            if(meta_codes & DAAP_META_PERSISTENTID)
                            {
                                //
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
                                // response << Tag('asbr') << (u16) ...
                            }

                            if(meta_codes & DAAP_META_SONGBEATSPERMINUTE)
                            {
                                // response << Tag('asbt') << (u16) ...
                            }
                        
                            if(meta_codes & DAAP_META_SONGCOMMENT)
                            {
                                // response << Tag('ascm') << utf8 ....
                            }

                            if(meta_codes & DAAP_META_SONGCOMPILATION)
                            {
                                response << Tag('asco') << (u8) 0 << end;
                            }
    
                            if(meta_codes & DAAP_META_SONGCOMPOSER)
                            {
                                // response << Tag('ascp') << utf8 ....
                            }

                            if(meta_codes & DAAP_META_SONGDATEADDED)
                            {
                                // response << Tag('asda') << (u32) ... (seconds since when ... ?)
                            }

                            if(meta_codes & DAAP_META_SONGDATEMODIFIED)
                            {
                                // response << Tag('asdm') << (u32) ... (seconds since when ... ?)
                            }

                            if(meta_codes & DAAP_META_SONGDISCCOUNT)
                            {
                                // response << Tag('asdc') << (u16) ...
                            }
                        
                            if(meta_codes & DAAP_META_SONGDISCNUMBER)
                            {
                                // response << Tag('asdn') << (u16) ...
                            }
                        
                            if(meta_codes & DAAP_META_SONGDISABLED)
                            {
                                //
                                //  Change this soon
                                //

                                if(which_item->getUrl().fileName().section('.', -1,-1) == "mp3")
                                {
                                    response << Tag('asdb') << (u8) 0 << end;
                                }
                                else
                                {
                                    response << Tag('asdb') << (u8) 1 << end;
                                }
                            }

                            if(meta_codes & DAAP_META_SONGEQPRESET)
                            {
                                //
                            }

                            if(meta_codes & DAAP_META_SONGFORMAT)
                            {
                                //
                                //  Change this soon
                                //

                                if(which_item->getUrl().fileName().section('.', -1,-1) == "mp3")
                                {
                                    response << Tag('asfm') << "mp3" << end;
                                }
                                else
                                {
                                    response << Tag('asfm') << "wav" << end;
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
                                // response << Tag('asdt') << utf8 ...
                            }
                        
                            if(meta_codes & DAAP_META_SONGRELATIVEVOLUME)
                            {
                                // response << Tag('asrv') << (u8) ...
                            }
                        
                            if(meta_codes & DAAP_META_SONGSAMPLERATE)
                            {
                                // response << Tag('assr') << (u32) ...
                            }
                        
                            if(meta_codes & DAAP_META_SONGSIZE)
                            {
                                // response << Tag('assz') << (u32) ...
                            }
                        
                            if(meta_codes & DAAP_META_SONGSTARTTIME)
                            {
                                response << Tag('asst') << (u32) 0 << end;
                            }

                            if(meta_codes & DAAP_META_SONGSTOPTIME)
                            {
                                response << Tag('assp') << (u32) which_item->getLength() << end;
                            }
                    
                            if(meta_codes & DAAP_META_SONGTIME)
                            {
                                response << Tag('astm') << (u32) which_item->getLength() << end;
                            }
                    
                            if(meta_codes & DAAP_META_SONGTRACKCOUNT)
                            {
                                // response << Tag('astc') << (u16) ...
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
                                // 
                            }
                    
                        
                            response << end;
                       }
                    }
                }
                else
                {
                    warning("daapserver got a metadata item off an audio collection that is not of type AudioMetadata");
                } 
             }
             response << end 
                      << end;
             parent->unlockMetadata();

    sendTag( http_request, response.data() );
}

void DaapServer::sendDatabaseItem(HttpRequest *http_request, u32 song_id)
{
    

    Metadata *which_one = parent->getMetadata(song_id);
    if(which_one)
    {
        if(which_one->getType() == MDT_audio)
        {
            AudioMetadata *which_audio = (AudioMetadata*)which_one;
            QUrl file_url = which_audio->getUrl();
            QString file_path = file_url.path();

            
            cout << "setting sendFile() to \"" << file_path << "\"" << endl;
            http_request->getResponse()->sendFile(file_path);

        }
        else
        {
            warning("daapserver got a reference to a non audio bit of metadata");
        }
    }
    else
    {
        warning("daapserver got a bad reference to a metadata item");
    }

}

void DaapServer::sendContainers(HttpRequest *http_request, DaapRequest *daap_request, int which_database)
{
    if(which_database != 1)
    {
        warning("daap server was asked about a database other than 1");
        return;
    }

    TagOutput response;
    response << Tag( 'aply' ) << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('muty') << (u8) 0 << end 
             << Tag('mtco') << (u32) 1 << end
             << Tag('mrco') << (u32) 1 << end 
             << Tag('mlcl');
             
             //
             // We always have 1 "fake" playlist, which always has id #1 and
             // is just all the metadata
             //
             
             response << Tag('mlit');
                
                if((u64) daap_request->getParsedMetaContentCodes() & DAAP_META_ITEMID)
                {
                    response << Tag('miid') << (u32) 1 << end ;
                }
             
                if((u64) daap_request->getParsedMetaContentCodes() & DAAP_META_ITEMNAME)
                {
                    response << Tag('minm') << service_name.utf8() << end ;
                }
             
                response << Tag('mimc') << (u32) parent->getAllAudioMetadataCount() << end ;
             response << end;
             
             //
             // For each container, explain it
             //
             
             MetadataContainer *a_container = NULL;
             for (   
                     a_container = metadata_containers->first(); 
                     a_container; 
                     a_container = metadata_containers->next() 
                 )
             {
                if(a_container->isAudio())
                {
            
            
                    QIntDict<Playlist> *which_playlists = a_container->getPlaylists();
                    QIntDictIterator<Playlist> it( *which_playlists );

                    Playlist *a_playlist;
                    while ( (a_playlist = it.current()) != 0 )
                    {
                        ++it;
             
                        response << Tag('mlit');
                
                        if((u64) daap_request->getParsedMetaContentCodes() & DAAP_META_ITEMID)
                        {
                            response << Tag('miid') << (u32) a_playlist->getId() << end ;
                        }
             
                        if((u64) daap_request->getParsedMetaContentCodes() & DAAP_META_ITEMNAME)
                        {
                            response << Tag('minm') << a_playlist->getName().utf8() << end ;
                        }
             
                        response << Tag('mimc') << (u32) a_playlist->getCount() << end ;
                        response << end;
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
        warning("daap server was asked about a database other than 1");
        return;
    }

    //
    //  Get the playlist in question, unless it's out special "all metadata" playlist (id = 1)
    //


    uint how_many;

    Playlist *the_playlist = NULL;
    
    if(container_id != 1)
    {
        the_playlist = parent->getPlaylist(container_id);
        if(!the_playlist)
        {
            warning("daapserver was asked for a playlist it can't find");
            return;
        }
        else
        {
            how_many = the_playlist->getCount();
        }
    }
    else
    {
        how_many = parent->getAllAudioMetadataCount();
    }
    
    TagOutput response;

    response << Tag( 'apso' ) << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('muty') << (u8) 0 << end 
             << Tag('mtco') << (u32) how_many << end 
             << Tag('mrco') << (u32) how_many << end 

             << Tag('mlcl');
             
                //
                //  For everthing inside this container
                //

                
                parent->lockMetadata();
                parent->lockPlaylists();

                if(container_id != 1)
                {
                    QValueList<uint> songs_in_list = the_playlist->getList();
                    typedef QValueList<uint> UINTList;
                    UINTList::iterator iter;
                    for ( iter = songs_in_list.begin(); iter != songs_in_list.end(); ++iter )
                    {
                            response << Tag('mlit') 
                                        << Tag('mikd') << (u8) 2  << end
                                        << Tag('miid') << (u32) (*iter) << end 
                                        << Tag('mcti') << (u32) (*iter) << end 
                                     << end;
                    }
                }
                else
                {
                    //
                    //  If it's playlist #1, that's *all* the metadata
                    //

                    MetadataContainer *a_container = NULL;
                    for (   
                            a_container = metadata_containers->first(); 
                            a_container; 
                            a_container = metadata_containers->next() 
                        )
                    {

                        if(a_container->isAudio())
                        {
                            QIntDict<Metadata>     *which_metadata;
                            which_metadata = a_container->getMetadata();

                            QIntDictIterator<Metadata> iterator(*which_metadata);
            
                            for( ; iterator.current(); ++iterator)
                            {
                                if(iterator.current()->getType() == MDT_audio)
                                {
                                    AudioMetadata *which_item = (AudioMetadata*)iterator.current();
                                    response << Tag('mlit') 
                                                << Tag('mikd') << (u8) 2  << end
                                                << Tag('miid') << (u32) which_item->getId() << end 
                                                << Tag('mcti') << (u32) which_item->getId() << end 
                                             << end;
                                }
                            }
                        }
                    }
                }

                parent->unlockPlaylists();
                parent->unlockMetadata();

             response << end 
             << end;

    sendTag( http_request, response.data() );

}

DaapServer::~DaapServer()
{
}
