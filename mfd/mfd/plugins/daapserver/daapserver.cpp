;/*
    daapserver.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    

*/

#include <qapplication.h>

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
    first_update = true;
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
    //InitParams initParams;


    //initParams.serverName = (char *) service_name.ascii();
    //initParams.dbName = (char *) service_name.ascii();

    //int argc = 0;
    //char *argv[1];
    //argv[0] = "";

    //initParams = *getConfigFile( argc, argv, initParams );
    //initParams = *readConfig( initParams );
    //initParams = *parseArgs( argc, argv, initParams );

    //if( initParams.dirs->size() == 0 ) {
    //    std::string dir( "." );
    //    initParams.dirs->push_back( dir );
    //}

    //cout << "scanning " << initParams.dirs << " for mp3s... " << flush;
    //Database db( initParams );
    //cout << "done. " << endl;



    server = httpdCreate(NULL, 3689);
    if(!server)
    {
        fatal("daapserver could not create a persistent http server ... will be unloaded");
    }

    //httpdSetAccessLog( server, stdout );
    //httpdSetErrorLog( server, stderr );

    // we do all URL parsing ourselves
    // httpdAddCSiteContent( server, parseRequest, (void*)&db );
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
    }   
    else if( daap_request->getRequestType() == DAAP_REQUEST_LOGIN)
    {
        u32 session_id = daap_sessions.getNewId();
        sendLogin( server, session_id);    
    }
    else if( daap_request->getRequestType() == DAAP_REQUEST_DATABASES)
    {
        parseVariables( server, daap_request);

        //
        //  If the session id (which we just parsed immediately above) is
        //  valid (ie. this client is already logged in), then proceed
        //
        
        if(daap_sessions.isValid(daap_request->getSessionId()))
        {
            //
            //  If this is not the first DB request, and the version of the
            //  database is not the same as our metadata says it is, then we
            //  need to tell the client it's store of data about *this* daap
            //  server is out of date.
            //

            // cout << "daap client requested  db #" << daap_request->getDatabaseVersion() << endl;
            // cout << "all_the_metadata is at db #" << all_the_metadata->getCurrentGeneration() << endl;
             
            
            if( daap_request->getDatabaseVersion() != 0 && 
                daap_request->getDatabaseVersion() != 
                (uint) all_the_metadata->getCurrentGeneration())
            {
                sendUpdate(server, all_the_metadata->getCurrentGeneration());
            }
            else 
            {
                //
                //  At some point, deal with deltas!!
                //
                sendMetadata( server, httpdRequestPath(server), daap_request);
            }

        }
    }
    else if (daap_request->getRequestType() == DAAP_REQUEST_UPDATE)
    {
        sendUpdate(server, all_the_metadata->getCurrentGeneration());
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
    
    response << Tag( 'msrv' ) << Tag('mstt') << (u32) DAAP_OK << end 
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
/*        Tag('mspi') <<
            (u8) 0 <<
        end <<
        Tag('msex') <<
            (u8) 0 <<
        end <<
        Tag('msbr') <<
            (u8) 0 <<
        end <<
        Tag('msqy') <<
            (u8) 0 <<
        end <<
        Tag('msix') <<
            (u8) 0 <<
        end <<
        Tag('msrs') <<
            (u8) 0 <<
        end <<
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
                //u32 container_id = components[3].toULong();
                sendContainer(server);

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
                    << Tag('mimc') << (u32) all_the_metadata->getMP3Count() << end 
             
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
             << Tag('mtco') << (u32) all_the_metadata->getMP3Count() << end 
             << Tag('mrco') << (u32) all_the_metadata->getMP3Count() << end 
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
                    if(iterator.current()->getUrl().fileName().section('.', -1,-1) == "mp3")
                    {
                        response << Tag('mlit');
                        if(meta_codes & DAAP_META_ITEMKIND)
                        {
                            response << Tag('mikd') << (u8) 2 << end;
                        }

                        if(meta_codes & DAAP_META_SONGDATAKIND)
                        {
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

                        if(meta_codes & DAAP_META_SONGFORMAT)
                        {
                            response << Tag('asfm') << "mp3" << end;
                        }
                    

                        response << end;
                    }
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
             
            response << Tag('mlit');
                if((u64) daap_request->getParsedMetaContentCodes() & DAAP_META_ITEMID)
                {
                    response << Tag('miid') << (u32) 1 << end ;
                }
             
                if((u64) daap_request->getParsedMetaContentCodes() & DAAP_META_ITEMNAME)
                {
                    response << Tag('minm') << QString("AnyOldPlaylist").utf8() << end ;
                }
             
                response << Tag('mimc') << (u32) all_the_metadata->getMP3Count() << end ;
            response << end;
             
             
             
             
             
             

             response << end
             << end;

    sendTag( server, response.data() );
}

void DaapServer::sendContainer(httpd *server)
{
    TagOutput response;

    response << Tag( 'apso' ) << Tag('mstt') << (u32) DAAP_OK << end 
             << Tag('muty') << (u8) 0 << end 
             << Tag('mtco') << (u32) all_the_metadata->getMP3Count() << end 
             << Tag('mrco') << (u32) all_the_metadata->getMP3Count() << end 
             << Tag('mlcl');
             
                //
                //  For everthing inside this container
                //
                current_metadata = all_the_metadata->getCurrentMetadata();

                QIntDictIterator<AudioMetadata> iterator(*current_metadata);
                for( ; iterator.current(); ++iterator)
                {
                    if(iterator.current()->getUrl().fileName().section('.', -1,-1) == "mp3")
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
