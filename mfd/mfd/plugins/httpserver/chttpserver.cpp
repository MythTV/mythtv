/*
	httpserver.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for the http server
*/

#include "settings.h"
#include "mfd_events.h"

#include "../../mdserver.h"

#include "chttpserver.h"
#include "httpinrequest.h"
#include "httpoutresponse.h"
#include "../../servicelister.h"
#include "settings.h"

ClientHttpServer *http_server = NULL;


ClientHttpServer::ClientHttpServer(MFD *owner, int identity)
      :MFDHttpPlugin(owner, identity, mfdContext->getNumSetting("mfd_httpufpi_port"), "http server", 1)
{
    core_table_columns = 3;

    //
    //  Find my metadata "server"
    //
    
    metadata_server = owner->getMetadataServer();

    other_ufpi.setAutoDelete(true);
}


void ClientHttpServer::run()
{
    //
    //  Register ourself as an available service
    //

    QString service_name = QString("UFPI on %1").arg(hostname); 

    //
    //  Register this (http) service
    //

    Service *http_service = new Service(
                                        QString("UFPI on %1").arg(hostname),
                                        QString("http"),
                                        hostname,
                                        SLT_HOST,
                                        (uint) port_number
                                       );
 
    ServiceEvent *se = new ServiceEvent( true, true, *http_service);
    QApplication::postEvent(parent, se);
    delete http_service;

    //
    //  Initialize our server socket
    //
    
    if(!initServerSocket())
    {
        warning(QString("could not init server socker on port %1")
                .arg(port_number));
    }

    
    //
    //  Become a client of the audio server (so we can control playback on
    //  _this_ box via http)
    //

    QHostAddress this_address;
    if(!this_address.setAddress("127.0.0.1"))
    {
        fatal("http server could not set address for 127.0.0.1");
        return;
    }

    client_socket_to_audio = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_audio->setBlocking(false);
    
    int connect_tries = 0;
    
    while(! (client_socket_to_audio->connect(this_address, mfdContext->getNumSetting("mfd_audio_port"))))
    {
        //
        //  We give this a few attempts. It can take a few on non-blocking sockets.
        //

        ++connect_tries;
        if(connect_tries > 10)
        {
            fatal("could not connect to the audio server as a regular client");
            return;
        }
        usleep(100);
    }
    
    while(keep_going)
    {
        updateSockets();
        checkThreadPool();
        checkServiceChanges();

        int nfds = 0;
        fd_set readfds;

        FD_ZERO(&readfds);


        //
        //  Add the server socket to things we want to watch
        //

        if(core_server_socket)
        {
            int a_socket = core_server_socket->socket();
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
                warning("core server socket invalid ... we are doomed");
            }
            
        }
        else
        {
            warning("core server socket gone ... death and destruction");
        }

        

        //
        //  Add all existing clients
        //

        client_sockets_mutex.lock();
            QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
            MFDServiceClientSocket *a_client;
            while ( (a_client = iterator.current()) != 0 )
            {
                ++iterator;
                int socket_fd = a_client->socket();
                if(socket_fd > 0)
                {
                    if(!a_client->isReading())
                    {
                        FD_SET(socket_fd, &readfds);
                        if(nfds <= socket_fd)
                        {
                            nfds = socket_fd + 1;
                        }
                    }
                }
            }
        client_sockets_mutex.unlock();

        //
        //  Add the control pipe
        //
            
        FD_SET(u_shaped_pipe[0], &readfds);
        if(nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    
        timeout_mutex.lock();
            timeout.tv_sec = 10;
            timeout.tv_usec = 0;
        timeout_mutex.unlock();

        //
        //  Sit in select() until data arrives
        //
    
        int result = select(nfds, &readfds, NULL, NULL, &timeout);
        if(result < 0)
        {
            warning("got an error on select()");
        }
    
        //
        //  In case data came in on out u_shaped_pipe, clean it out
        //

        if(FD_ISSET(u_shaped_pipe[0], &readfds))
        {
            u_shaped_pipe_mutex.lock();
                char read_back[2049];
                read(u_shaped_pipe[0], read_back, 2048);
            u_shaped_pipe_mutex.unlock();
        }
        
        //
        //  Clean out (and ignore) anything the audio server is sending back
        //  to us
        //
        
        char read_back[2049];
        client_socket_to_audio->readBlock(read_back, 2048);
    
    }
}    

void ClientHttpServer::handleServiceChange()
{
    //
    //  Something has changed in available services. I query the mfd,
    //  looking for other http services (so I can build a page on _this_ box
    //  that includes links to any other http service on the network that
    //  includes the phrase UFPI in its description)
    //
    
    ServiceLister *service_lister = parent->getServiceLister();
    
    service_lister->lockDiscoveredList();

        //
        //  Go through my list of http instances and make sure they still
        //  exist in the mfd's service list
        //

        other_ufpi_mutex.lock();

            QPtrListIterator<OtherUFPI> iter( other_ufpi );
            OtherUFPI *a_ufpi;
            while ( (a_ufpi = iter.current()) != 0 )
            {
                ++iter;
                if( !service_lister->findDiscoveredService( a_ufpi->getName()) )
                {
                    log(QString("noticed that a UFPI service went away: "
                                "\"%1\" (%2)")
                                .arg(a_ufpi->getName())
                                .arg(a_ufpi->getUrl()), 9);
                    other_ufpi.remove(a_ufpi);    
                }
            }
        other_ufpi_mutex.unlock();
            

        //
        //  Go through services on the master list, add any that are HTTP
        //  and have UFPI in the name (just add them all, as the add()
        //  method with check for dupes.
        //
        
        typedef     QValueList<Service> ServiceList;
        ServiceList *discovered_list = service_lister->getDiscoveredList();
        

        ServiceList::iterator it;
        for ( it  = discovered_list->begin(); 
              it != discovered_list->end(); 
              ++it )
        {
            //
            //  We just try and add any that are not on this host, as the
            //  addDaapServer() method checks for dupes
            //
                
            if ( (*it).getType() == "http")
            {
                if( (*it).getLocation() == SLT_LAN ||
                    (*it).getLocation() == SLT_NET )
                {
                    if( (*it).getName().contains("UFPI"))
                    {
                        addMHttpServer( (*it).getAddress(), (*it).getPort(), (*it).getName() );
                    }
                }
            }
        }


    service_lister->unlockDiscoveredList();
}

void ClientHttpServer::addMHttpServer(QString server_address, uint server_port, QString service_name)
{
    //
    //  Sometimes these get passed twice, especially at startup, only add we don't have it already
    //

    other_ufpi_mutex.lock();

        QPtrListIterator<OtherUFPI> it( other_ufpi );
        OtherUFPI *which_one = NULL;
        OtherUFPI *a_ufpi;
        while ( (a_ufpi = it.current()) != 0 )
        {
            ++it;
            if(a_ufpi->getName() == service_name)
            {
                which_one = a_ufpi;
                break;
            }
        }
        if(which_one)
        {
            //
            //  Do nothing
            //
        }
        else
        {
            log(QString("httpserver has noticed a new http service: \"%1\" (%2:%3)")
                        .arg(service_name)
                        .arg(server_address)
                        .arg(server_port), 10);

            QString constructed_url = QString("http://%1:%2/").arg(server_address).arg(server_port);

            OtherUFPI *new_ufpi = new OtherUFPI(service_name, constructed_url);

            other_ufpi.append(new_ufpi);
        }

    other_ufpi_mutex.unlock();
}

void ClientHttpServer::handleIncoming(HttpInRequest *http_request, int)
{
    //
    //  Get the branches of the get, and default to /audio/playlists if there's nothing there
    //
    
    QString get_request = http_request->getUrl();
    QString branch_one = get_request.section('/',1,1);
    QString branch_two = get_request.section('/',2,2);
    if(branch_one.length() < 1)
    {
        branch_one = "audio";
        branch_two = "playlists";
    }

    //
    //  See if we have any GET variables that are trying to tell us to do
    //  something
    //
    
    QString playtrack_command    = http_request->getVariable("playtrack");
    QString playplaylist_command = http_request->getVariable("playplaylist");
    QString container_command    = http_request->getVariable("container");
    QString stop_command         = http_request->getVariable("stopcommand");
    
    if(playtrack_command.length() > 0 && container_command.length() > 0)
    {
        //
        //  
        //
        
        int track_id = playtrack_command.toInt();
        int container_id = container_command.toInt();
        if(track_id > 0 && container_id > 0)
        {
            playTrack(container_id, track_id);
        }
        
    }

    if(playplaylist_command.length() > 0 && container_command.length() > 0)
    {
        //
        //  
        //
        
        int playlist_id = playplaylist_command.toInt();
        int container_id = container_command.toInt();
        if(playlist_id > 0 && container_id > 0)
        {
            playPlaylist(container_id, playlist_id);
        }
        
    }
    
    if(stop_command.length() > 0)
    {
        if(stop_command == "stop")
        {
            stopAudio();
        }
        if(stop_command == "prev")
        {
            prevAudio();
        }
        if(stop_command == "next")
        {
            nextAudio();
        }
    }


    //
    //  Build our response (the http_request automatically has an http
    //  response attached to it)
    //

    QTime page_build_time;
    page_build_time.start();
    
    HttpOutResponse *response = http_request->getResponse();
    
    addTopHTML(response);
    startCoreTable(response);
    listMFDs(response);
    listSections(response, branch_one);
    showCurrentSection(response, branch_one, branch_two);
    endCoreTable(response);
    addBottomHTML(response);

    log(QString("built HTML response for client in %1 second(s)")
        .arg(page_build_time.elapsed() / 1000.0), 9);    
    //
    //  And we're done. Our parent class takes care of sending the response
    //
}

void ClientHttpServer::addTopHTML(HttpOutResponse *response)
{
    response->addHeader("Content-Type: text/html");
    response->clearPayload();
    
    //
    //  Just building up the HTML as we go here ... pretty hacky
    //

    response->addToPayload("<html xmlns=\"http://www.w3.org/1999/xhtml\">");
    response->addToPayload("<head>");
    response->addToPayload(QString("<title>MFD on %1</title>").arg(hostname));
    response->addToPayload("</head>");
    response->addToPayload("<body bgcolor=\"#FFFFFF\" text=\"#000000\" link=\"#0000FF\" vlink=\"#000080\" alink=\"#FF0000\">");
    response->addToPayload("<center>");
    response->addToPayload("<p>MFD Ultra Fancy Pants Interface</p>");
    response->addToPayload("</center>");
    response->addToPayload("<hr><br><br>");
}

void ClientHttpServer::startCoreTable(HttpOutResponse *response)
{
    response->addToPayload("<center>");
    response->addToPayload("<table>");
}

void ClientHttpServer::listMFDs(HttpOutResponse *response)
{
    response->addToPayload("<tr>");
    response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
    response->addToPayload(QString("UFPI on %1").arg(mfdContext->getHostName()));

    other_ufpi_mutex.lock();
        for(uint i = 0; i < other_ufpi.count(); i++)
        {
            response->addToPayload(QString(" | <a href=\"%1\">%2</a>")
            .arg(other_ufpi.at(i)->getUrl())
            .arg(other_ufpi.at(i)->getName())
            );
        }
    other_ufpi_mutex.unlock();

    response->addToPayload("</td>");
    response->addToPayload("</tr>");

    response->addToPayload("<tr>");
    response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
    response->addToPayload("<hr>");
    response->addToPayload("</td>");
    response->addToPayload("</tr>");
}

void ClientHttpServer::listSections(HttpOutResponse *response, const QString &branch_one)
{
    response->addToPayload("<tr>");
    response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));

    //
    //  Add the various "sections", with an HREF iff it's not the section we're currently in
    //
    
    if(branch_one == "status")
    {
        response->addToPayload("Status ");
    }
    else
    {
        response->addToPayload("<a href=\"/status\">Status</a> ");
    }

    if(branch_one == "audio")
    {
        response->addToPayload("| Audio ");
    }
    else
    {
        response->addToPayload("| <a href=\"/audio/playlists\">Audio</a> ");
    }

    if(branch_one == "video")
    {
        response->addToPayload("| Video ");
    }
    else
    {
        response->addToPayload("| <a href=\"/video\">Video</a> ");
    }

    if(branch_one == "images")
    {
        response->addToPayload("| Images ");
    }
    else
    {
        response->addToPayload("| <a href=\"/images\">Images</a> ");
    }

    response->addToPayload("</td>");
    response->addToPayload("</tr>");

    response->addToPayload("<tr>");
    response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
    response->addToPayload("<hr>");
    response->addToPayload("</td>");
    response->addToPayload("</tr>");

}

void ClientHttpServer::showCurrentSection(HttpOutResponse *response, const QString &branch_one, const QString &branch_two)
{

    //
    //  Do the menu bit for this section
    //

    if(branch_one == "status" || branch_one == "video" || branch_one == "images")
    {
        response->addToPayload("<tr>");
        response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
        response->addToPayload("Doesn't | Do | Anything | Yet");
        response->addToPayload("</td>");
        response->addToPayload("</tr>");

        response->addToPayload("<tr>");
        response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
        response->addToPayload("<hr>");
        response->addToPayload("</td>");
        response->addToPayload("</tr>");
        
        response->addToPayload("<tr><td>&nbsp</td><td>&nbsp</td><td>&nbsp</td></tr>");
        response->addToPayload("<tr><td>&nbsp</td><td align=\"center\">Not Yet</td><td>&nbsp</td></tr>");
        response->addToPayload("<tr><td>&nbsp</td><td>&nbsp</td><td>&nbsp</td></tr>");
        
    }
    else if(branch_one == "audio")
    {
        if(branch_two == "playlists")
        {
            response->addToPayload("<tr>");
            response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
            response->addToPayload("Playlists | <a href=\"/audio/tracks\">Tracks</a> ");
            response->addToPayload("</td>");
            response->addToPayload("</tr>");

            response->addToPayload("<tr>");
            response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
            response->addToPayload("<hr>");
            response->addToPayload("</td>");
            response->addToPayload("</tr>");
            
            response->addToPayload("<tr>");

            response->addToPayload("<td colspan=\"3\" align=\"center\">");

            response->addToPayload(" <a href=\"/audio/playlists?stopcommand=prev\">PREV</a> ");
            response->addToPayload(" &nbsp; &nbsp; <a href=\"/audio/playlists?stopcommand=stop\">STOP</a> ");
            response->addToPayload(" &nbsp; &nbsp; <a href=\"/audio/playlists?stopcommand=next\">NEXT</a> ");

            response->addToPayload("</td>");

            response->addToPayload("</tr>");

            showPlaylists(response);
        }
        else if(branch_two == "tracks")
        {
            response->addToPayload("<tr>");
            response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
            response->addToPayload("<a href=\"/audio/playlists\">Playlists</a> | Tracks ");
            response->addToPayload("</td>");
            response->addToPayload("</tr>");

            response->addToPayload("<tr>");
            response->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
            response->addToPayload("<hr>");
            response->addToPayload("</td>");
            response->addToPayload("</tr>");

            response->addToPayload("<tr>");
            response->addToPayload("<td colspan=\"3\" align=\"center\"><a href=\"/audio/tracks?stopcommand=stop\">STOP</a></td>");
            response->addToPayload("</td>");
            response->addToPayload("</tr>");



            showTracks(response);

        }
        else
        {
            response->setError(404);
        }
    }
    else
    {
        response->setError(404);
    }

}

void ClientHttpServer::showPlaylists(HttpOutResponse *response)
{
    //
    //  Lock the metadata
    //
    
    metadata_server->lockMetadata();
    
    //
    //  Get a pointer to the metadata containers
    //
    
    QPtrList<MetadataContainer> *metadata_containers = metadata_server->getMetadataContainers();

    //
    //  Iterate over the metadata containers, and send out all the playlists
    //
    
    
    int counter = 0;
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
                    if(counter % 2 == 0)
                    {
                        response->addToPayload("<tr bgcolor=\"ccccee\">");
                    }
                    else
                    {
                        response->addToPayload("<tr>");
                    }
                    ++counter;

                    response->addToPayload("<td>");
                    response->addToPayload(a_playlist->getName());
                    response->addToPayload("</td>");

                    response->addToPayload("<td>");
                    response->addToPayload(QString("&nbsp; %1 track(s) &nbsp;").arg(a_playlist->getFlatCount()));
                    response->addToPayload("</td>");

                    response->addToPayload("<td>");
                    response->addToPayload(
                        QString("<a href=\"/audio/playlists?playplaylist=%1&container=%2\">play</a>")
                        .arg(a_playlist->getId())
                        .arg(a_container->getIdentifier()));
                    response->addToPayload("</td>");

                    response->addToPayload("</tr>");
                }
            }
        }
    }
    
    //
    //  Unlock the metadata
    //    

    metadata_server->unlockMetadata();

}

void ClientHttpServer::showTracks(HttpOutResponse *response)
{
    //
    //  Lock the metadata
    //
    
    metadata_server->lockMetadata();
    
    //
    //  Get a pointer to the metadata containers
    //
    
    QPtrList<MetadataContainer> *metadata_containers = metadata_server->getMetadataContainers();

    //
    //  Iterate over the metadata containers, and send out all the playlists
    //
    
    int counter = 0;
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
                        if(AudioMetadata *which_item = dynamic_cast<AudioMetadata*>(iterator.current()) )
                        {
                            if(!a_container->isLocal() && metadata_server->getLocalEquivalent(which_item))
                            {
                                //
                                //  No need to list this, as a local equivalent will appear instead
                                //
                            }
                            else
                            {
                                if(counter % 2 == 0)
                                {
                                    response->addToPayload("<tr bgcolor=\"ccccee\">");
                                }
                                else
                                {
                                    response->addToPayload("<tr>");
                                }
                                ++counter;

                                response->addToPayload("<td>");
                                response->addToPayload(which_item->getArtist());
                                response->addToPayload("</td>");

                                response->addToPayload("<td>");
                                response->addToPayload(which_item->getTitle());
                                response->addToPayload("</td>");

                                response->addToPayload("<td>");
                                response->addToPayload(
                                    QString("<a href=\"/audio/tracks?playtrack=%1&container=%2\">play</a>")
                                    .arg(which_item->getId())
                                    .arg(a_container->getIdentifier()));
                                response->addToPayload("</td>");

                                response->addToPayload("</tr>");
                            }
                        }
                        else
                        {
                            warning("getting non audio metadata out of an audio metdata container");
                        }
                    }
                }
            }
        }
    }
    
    //
    //  Unlock the metadata
    //    

    metadata_server->unlockMetadata();

}

void ClientHttpServer::endCoreTable(HttpOutResponse *response)
{
    response->addToPayload("</table>");
    response->addToPayload("</center>");
}

void ClientHttpServer::addBottomHTML(HttpOutResponse *response)
{
    response->addToPayload("<br><br><hr>");
    response->addToPayload("<font size=\"-2\">");
    response->addToPayload("<center>");
    response->addToPayload("<p>&copy 2002, 2003, 2004 &nbsp by Isaac Richards, Thor Sigvaldason, and all the <a href=\"http://www.mythtv.org/\">MythTV</a> contributors.</p>");
    response->addToPayload("</center>");
    response->addToPayload("</font>");
}


void ClientHttpServer::playTrack(int which_container, int which_track)
{
    QString playing_command = QString("play item %1 %2").arg(which_container).arg(which_track);
    client_socket_to_audio->writeBlock(playing_command.ascii(), playing_command.length());
}

void ClientHttpServer::playPlaylist(int which_container, int which_playlist)
{
    QString playing_command = QString("play container %1 %2").arg(which_container).arg(which_playlist);
    client_socket_to_audio->writeBlock(playing_command.ascii(), playing_command.length());
}

void ClientHttpServer::stopAudio()
{
    QString stop_command = "stop";
    client_socket_to_audio->writeBlock(stop_command.ascii(), stop_command.length());
}

void ClientHttpServer::prevAudio()
{
    QString stop_command = "previous";
    client_socket_to_audio->writeBlock(stop_command.ascii(), stop_command.length());
}

void ClientHttpServer::nextAudio()
{
    QString stop_command = "next";
    client_socket_to_audio->writeBlock(stop_command.ascii(), stop_command.length());
}

ClientHttpServer::~ClientHttpServer()
{
}

