;/*
    daapserver.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    

*/

#include <qapplication.h>
#include <qvaluelist.h>

#include "mfd_events.h"

#include "daapserver.h"
#include "mcc_bitfield.h"


#include <iostream>
using namespace std;

#include "daaplib/tagoutput.h"
#include "daaplib/registry.h"

#include "./httpd_persistent/httpd.h"

DaapServer *daap_server = NULL;


const int   DAAP_OK = 200;


extern "C" void handleSelectCallback(bool *continue_flag)
{
    *continue_flag = daap_server->wantsToContinue();
}


extern "C" void parseRequest( httpd *server, void *)
{

    daap_server->parseIncomingRequest(server);
}

/*
---------------------------------------------------------------------
*/


DaapServer::DaapServer(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, 3689)
{
    all_the_metadata = owner->getMetadataContainer();
    
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
}

void DaapServer::run()
{
    //
    //  Register this (daap) service
    //

    ServiceEvent *se = new ServiceEvent(QString("services add daap %1 %2")
                                       .arg(port_number)
                                       .arg(service_name));
    QApplication::postEvent(parent, se);

    //
    //  Create an httpd server (hey .. wadda ya need Apache for?)
    //

    httpd *server;

    server = httpdCreate(NULL, 3689);
    if(!server)
    {
        fatal("daapserver could not create a persistent http server ... will be unloaded");
    }

    //
    //  You can uncomment these to get libhttp to log to the console
    //

    //httpdSetAccessLog( server, stdout );
    //httpdSetErrorLog( server, stderr );

    //
    // Tell libhttp we want to parse all requests ourself
    //
    
    httpdAddCSiteContent( server, parseRequest, (void*)0);
    httpdSetContentType( server, "application/x-dmap-tagged" );

    //
    //  Ask the select loop in httpd to call us each time through
    //  the iterations
    //
    
    httpdAddSelectCallback(server, handleSelectCallback);
    

    struct timeval timeout;

    while(keep_going)
    {
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;

        //
        //  This SelectLoop will pop a callback to handleSelectCallback()
        //  every tv_sec's, where we can tell it to stop if needed
        //
        
        httpdSelectLoop( server, timeout);
    }
    
    //
    //  For plugin reloading to work, we need to clean up after httpd
    //

    httpdDestroy(server);
    server = NULL;
}

void DaapServer::parseIncomingRequest(httpd *server)
{


    //
    //  Create a DaapRequest object that will be built up to understand the
    //  request that has just come in from a client
    //

    DaapRequest *daap_request = new DaapRequest();

    //
    //  Start to build up the request by parsing the path of the request.
    //
    
    parsePath(server, daap_request);
    
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
        sendServerInfo(server);
        return;
    }   
    else if( daap_request->getRequestType() == DAAP_REQUEST_LOGIN)
    {
        u32 session_id = daap_sessions.getNewId();
        sendLogin( server, session_id);
        return;
    }

    //
    //  IF we've made it this far, then we have a logged in client, so we
    //  should be able to parse the request to get session ID, etc.
    //

    parseVariables( server, daap_request);

    if(!daap_sessions.isValid(daap_request->getSessionId()))
    {
        //
        //  Hmmm ... this isn't right. The DAAP client did not ask for
        //  server info or a login, but it does not have a valid session id
        //  (which it can get by requesting a login).
        //
        
        httpdSend403( server );
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

        if(daap_request->getDatabaseVersion() != all_the_metadata->getCurrentGeneration() &&
           daap_request->getDatabaseVersion() != 0)
        {
            log(QString("daap client asked for a database request, "
                        "but has stale db reference "
                        "(mfd db# = %1, client db# = %2)")
                        .arg(all_the_metadata->getCurrentGeneration())
                        .arg(daap_request->getDatabaseVersion()), 9);

            sendUpdate(server, all_the_metadata->getCurrentGeneration());
        }
        else
        {
            sendMetadata( server, httpdRequestPath(server), daap_request);
        }        
        return;        
    }
    else if (daap_request->getRequestType() == DAAP_REQUEST_UPDATE)
    {
        //
        //  iTunes has an annoying habit of just sending these requests. We
        //  should only respond if the database version numbers are out of
        //  whack.
        //

        if(daap_request->getDatabaseVersion() != all_the_metadata->getCurrentGeneration())
        {
            log(QString("daap client asked for and will get an update "
                        "(mfd db# = %1, client db# = %2)")
                        .arg(all_the_metadata->getCurrentGeneration())
                        .arg(daap_request->getDatabaseVersion()), 9);
            sendUpdate(server, all_the_metadata->getCurrentGeneration());
        }
    }
    
    delete daap_request;
    
}


void DaapServer::parsePath(httpd *server, DaapRequest *daap_request)
{
    QString the_path = httpdRequestPath(server);

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

        httpdSend400( server );

    }
}

void DaapServer::sendServerInfo(httpd *server)
{
    Version daapVersion( 2, 0 );
    Version dmapVersion( 1, 0 );
    double version = httpdRequestDaapVersion( server );

    if( version == 1.0 ) {
        daapVersion.hi = 1;
        daapVersion.lo = 0;
    } 
    
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
             <<
// Disable some features for now
/*              Tag('mspi') << (u8) 0 << end 
             << Tag('msex') << (u8) 0 << end 
             << Tag('msbr') << (u8) 0 << end 
             << Tag('msqy') << (u8) 0 << end 
             << Tag('msix') << (u8) 0 << end 
             << Tag('msrs') << (u8) 0 << end 
             <<
*/        
                Tag('msdc') << (u32) 1 << end 
             
             << end;

    sendTag( server, response.data() );
    
}

void DaapServer::sendTag( httpd* server, const Chunk& c ) 
{
    httpdSetContentType( server, "application/x-dmap-tagged" );
    httpdWrite( server, (char*) c.begin(), c.size() );
}

void DaapServer::sendLogin(httpd *server, u32 session_id)
{
    TagOutput response;
    response << Tag('mlog') << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('mlid') << (u32) session_id << end 
             << end;
    sendTag( server, response.data() );
}

void DaapServer::parseVariables(httpd *server, DaapRequest *daap_request)
{
    bool ok;
    httpVar *varPtr;

    //
    //  Check the user agent header that httpd returns to take note of which
    //  kind of client this is (for iTunes we (will) need to re-encode some
    //  content on the fly.
    //

    QString user_agent = server->request.userAgent;
    if(user_agent.contains("iTunes/4"))
    {
        daap_request->setClientType(DAAP_CLIENT_ITUNES4X);
    }
    

    //
    //  This goes through the data that came in on the client request,
    //  breaks it up into useful pieces, and assigns them to our DaapRequest
    //  object
    //


    if ( ( varPtr = httpdGetVariableByName ( server, "session-id" ) ) != NULL)
    {
        daap_request->setSessionId(QString(varPtr->value).toULong(&ok));
        if(!ok)
            warning("daapserver failed to parse session id from client request");
    }

    if ( ( varPtr = httpdGetVariableByName ( server, "revision-number" ) ) != NULL)
    {
        daap_request->setDatabaseVersion(QString(varPtr->value).toULong(&ok));
        if(!ok)
            warning("daapserver failed to parse database version from client request");
    }

    if ( ( varPtr = httpdGetVariableByName ( server, "delta" ) ) != NULL)
    {
        daap_request->setDatabaseDelta(QString(varPtr->value).toULong(&ok));
        if(!ok)
            warning("daapserver failed to parse database delta from client request");
    }

    if ( ( varPtr = httpdGetVariableByName ( server, "type" ) ) != NULL)
    {
        daap_request->setContentType(QString(varPtr->value));
    }
    
    if ( ( varPtr = httpdGetVariableByName ( server, "meta" ) ) != NULL)
    {
        daap_request->setRawMetaContentCodes(QStringList::split(",",QString(varPtr->value)));
        daap_request->parseRawMetaContentCodes();
    }

    if ( ( varPtr = httpdGetVariableByName ( server, "filter" ) ) != NULL)
    {
        daap_request->setFilter(QString(varPtr->value));
    }
    
    if ( ( varPtr = httpdGetVariableByName ( server, "query" ) ) != NULL)
    {
        daap_request->setQuery(QString(varPtr->value));
    }
    
    if ( ( varPtr = httpdGetVariableByName ( server, "index" ) ) != NULL)
    {
        daap_request->setIndex(QString(varPtr->value));
    }
}

void DaapServer::sendUpdate(httpd *server, u32 database_version)
{

/*
    //
    //  Terrible HACK (temporary)
    //

    if(database_version !=3)
    {
        database_version = 3;
    }

    //
    //  Even worse HACK
    //

    if(!first_update)
    {
        return;
    }

    first_update = false;
*/

    TagOutput response;

    response << Tag( 'mupd' ) << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('musr') << (u32) database_version << end 
             << end;

    sendTag( server, response.data() );
    
}

void DaapServer::sendMetadata(httpd *server, QString request_path, DaapRequest *daap_request)
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
        
        sendDatabaseList( server);
    } 
    else 
    {
        //
        //  Ok, it's asking for something below /databases/...
        //

        /*
        if( components[1].toULong() != (ulong) all_the_metadata->getCurrentGeneration() ||
            components.count() < 3)
        {
            //
            //  I don't know what it's asking for ... doesn't make sense
            //
            
            httpdSend403(server);
            return;
        }
        */ 
        if( components[2] == "items" )
        {
            if( components.count() == 3 )
            {
                sendDatabase( server, daap_request);
            } 
            else if ( components.count() == 4 )
            {
                QString reference_to_song = components[3];
                QString cut_down = reference_to_song.section('.',-2, -2);
                bool ok;
                u32 song_id = cut_down.toULong(&ok);
                if(ok)
                {
                    sendDatabaseItem(server, song_id);
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
                sendContainers( server, daap_request);
            } 
            else if ( components.count() == 5 && components[4] == "items" )
            {
                u32 container_id = components[3].toULong();
                sendContainer(server, container_id);

            } 
            else 
            {
                httpdSend403( server );
                return;
            }
        } 
        else 
        {
            httpdSend403( server );
            return;
        }
    }    
}

void DaapServer::sendDatabaseList( httpd *server)
{
    all_the_metadata = parent->getMetadataContainer();

    TagOutput response;
    response << Tag('avdb') << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('muty') << (u8) 0 << end 
             << Tag('mtco') << (u32) 1 << end 
             << Tag('mrco') << (u32) 1 << end 
             << Tag('mlcl') 
                << Tag('mlit') 
                    << Tag('miid') << (u32) 1 << end 

                    // << Tag('mper') << (u64) 543675286654 << end 

                    << Tag('minm') << service_name.utf8() << end 
                    << Tag('mimc') << (u32) all_the_metadata->getMetadataCount() << end 
             
                    //
                    // Change this soon to add playlists
                    //
                    
                    << Tag('mctc') << (u32) 1 << end 
                << end 
             << end 
             << end;

    sendTag( server, response.data() );

}

void DaapServer::sendDatabase(httpd *server, DaapRequest *daap_request)
{ 
    TagOutput response;
    response << Tag( 'adbs' ) << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('muty') << (u8) 0 << end 
             << Tag('mtco') << (u32) all_the_metadata->getMetadataCount() << end 
             << Tag('mrco') << (u32) all_the_metadata->getMetadataCount() << end 
             << Tag('mlcl') ;
             
             //
             // We have to put in a record for every song, adding metadata
             // according to what the client requested.
             //
             
             u64 meta_codes = daap_request->getParsedMetaContentCodes();
             all_the_metadata->lockMetadata();
                current_metadata = all_the_metadata->getCurrentMetadata();
                QIntDictIterator<AudioMetadata> iterator(*current_metadata);
                for( ; iterator.current(); ++iterator)
                {
                    //if(iterator.current()->getUrl().fileName().section('.', -1,-1) == "mp3")
                    //{
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
                            response << Tag('minm') << iterator.current()->getTitle().utf8() << end;
                        }

                        if(meta_codes & DAAP_META_ITEMID)
                        {
                            response << Tag('miid') << (u32) iterator.current()->getId() << end;
                        }
                    
                        if(meta_codes & DAAP_META_PERSISTENTID)
                        {
                            //
                        }
                    
                        if(meta_codes & DAAP_META_SONGALBUM)
                        {
                            response << Tag('asal') << iterator.current()->getAlbum().utf8() << end;
                        }
                    
                        if(meta_codes & DAAP_META_SONGARTIST)
                        {
                            response << Tag('asar') << iterator.current()->getArtist().utf8() << end;
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

                            if(iterator.current()->getUrl().fileName().section('.', -1,-1) == "mp3")
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

                            if(iterator.current()->getUrl().fileName().section('.', -1,-1) == "mp3")
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
                            if(iterator.current()->getGenre().length() > 0 &&
                               iterator.current()->getGenre() != "Unknown" &&
                               iterator.current()->getGenre() != "Unknown Genre")
                            {
                                response << Tag('asgn') << iterator.current()->getGenre().utf8() << end;
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
                            response << Tag('assp') << (u32) iterator.current()->getLength() << end;
                        }
                    
                        if(meta_codes & DAAP_META_SONGTIME)
                        {
                            response << Tag('astm') << (u32) iterator.current()->getLength() << end;
                        }
                    
                        if(meta_codes & DAAP_META_SONGTRACKCOUNT)
                        {
                            // response << Tag('astc') << (u16) ...
                        }

                        if(meta_codes & DAAP_META_SONGTRACKNUMBER)
                        {
                            response << Tag('astn') << (u16) iterator.current()->getTrack() << end;
                        }
                    
                        if(meta_codes & DAAP_META_SONGUSERRATING)
                        {
                            response << Tag('asur') << (u8) (iterator.current()->getRating() * 10) << end;
                        }

                        if(meta_codes & DAAP_META_SONGYEAR)
                        {
                            response << Tag('asyr') << (u16) iterator.current()->getYear() << end;
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
                    //}
                } 
             all_the_metadata->unlockMetadata();
             
             response << end 
                      << end;

    sendTag( server, response.data() );
    
}

void DaapServer::sendDatabaseItem(httpd *server, u32 song_id)
{
    current_metadata = all_the_metadata->getCurrentMetadata();
    AudioMetadata *which_one = current_metadata->find(song_id);
    if(which_one)
    {
        QUrl file_url = which_one->getUrl();
        QString file_path = file_url.path();
        httpdSendFile( server, (char *) file_path.ascii(), httpdRequestContentRange( server ) );
    }
    else
    {
        cout << "WTF is wrong with looking for " << song_id << endl;
    }
    
}

void DaapServer::sendContainers(httpd *server, DaapRequest *daap_request)
{
    TagOutput response;
    response << Tag( 'aply' ) << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('muty') << (u8) 0 << end 
             << Tag('mtco') << (u32) 1 << end
             << Tag('mrco') << (u32) 1 << end 
             << Tag('mlcl');
             
             //
             // For each container, explain it
             //
             
             current_playlists = all_the_metadata->getCurrentPlaylists();
             
             QPtrListIterator<MPlaylist> it( *current_playlists );
             MPlaylist *a_playlist;
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
             
             
             
             
             
             

             response << end
             << end;

    sendTag( server, response.data() );
}

void DaapServer::sendContainer(httpd *server, u32 container_id)
{
    TagOutput response;

    //
    //  Find the playlist in question
    //

    QPtrListIterator<MPlaylist> it( *current_playlists );
    MPlaylist *the_playlist = NULL;
    MPlaylist *a_playlist;
    while ( (a_playlist = it.current()) != 0 )
    {
        if(a_playlist->getId() == container_id)
        {
            the_playlist = it.current();
        }
        ++it;
    }
    if(!the_playlist)
    {
        return;
    }
    
    

    response << Tag( 'apso' ) << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('muty') << (u8) 0 << end 
             << Tag('mtco') << (u32) the_playlist->getCount() << end 
             << Tag('mrco') << (u32) the_playlist->getCount() << end 
             << Tag('mlcl');
             
                //
                //  For everthing inside this container
                //

                
                if(the_playlist->getId() > 1)
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
                    current_metadata = all_the_metadata->getCurrentMetadata();

                    QIntDictIterator<AudioMetadata> iterator(*current_metadata);
                    for( ; iterator.current(); ++iterator)
                    {
                        response << Tag('mlit') 
                                    << Tag('mikd') << (u8) 2  << end
                                    << Tag('miid') << (u32) iterator.current()->getId() << end 
                                    << Tag('mcti') << (u32) iterator.current()->getId() << end 
                                 << end;
                    }
                }
                
             response << end 
             << end;

    sendTag( server, response.data() );

}



DaapServer::~DaapServer()
{
}
