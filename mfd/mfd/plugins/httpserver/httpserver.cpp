/*
	httpserver.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for the http server
*/

#include "settings.h"
#include "mfd_events.h"

#include "../../mdserver.h"

#include "httpserver.h"
#include "httprequest.h"
#include "httpresponse.h"


ClientHttpServer *http_server = NULL;


ClientHttpServer::ClientHttpServer(MFD *owner, int identity)
      :MFDHttpPlugin(owner, identity, 2345, "http server", 1)
{
    my_hostname = mfdContext->getHostName();
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

    QString service_name = QString("UFPI on %1").arg(mfdContext->getHostName()); 

    //
    //  Register this (mhttp) service
    //

    ServiceEvent *se = new ServiceEvent(QString("services add mhttp %1 %2")
                                       .arg(port_number)
                                       .arg(service_name));
    QApplication::postEvent(parent, se);


    //
    //  Initialize our server socket
    //
    
    if(!initServerSocket())
    {
        warning(QString("could not init server socker on port %1")
                .arg(port_number));
    }

    

    //
    //  Get a socket to the core mfd for service announcemnts
    //
    
    QHostAddress this_address;
    if(!this_address.setAddress("127.0.0.1"))
    {
        fatal("http server could not set address for 127.0.0.1");
        return;
    }

    client_socket_to_mfd = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_mfd->setBlocking(false);
    
    int connect_tries = 0;
    
    while(! (client_socket_to_mfd->connect(this_address, 2342)))
    {
        //
        //  We give this a few attempts. It can take a few on non-blocking sockets.
        //

        ++connect_tries;
        if(connect_tries > 10)
        {
            fatal("could not connect to the core mfd as a regular client");
            return;
        }
        usleep(100);
    }
    

    QString first_and_only_command = "services list";
    client_socket_to_mfd->writeBlock(first_and_only_command.ascii(), first_and_only_command.length());
    
    //
    //  Become a client of the audio server
    //

    client_socket_to_audio = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_audio->setBlocking(false);
    
    connect_tries = 0;
    
    while(! (client_socket_to_audio->connect(this_address, 2343)))
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

        int nfds = 0;
        fd_set readfds;

        FD_ZERO(&readfds);


        //
        //  Add the socket to the mfd (where we find out about new services,
        //  services going away, etc.)
        //

        int a_socket = client_socket_to_mfd->socket();
        if(a_socket > 0)
        {
            FD_SET(a_socket, &readfds);
            if(nfds <= a_socket)
            {
                nfds = a_socket + 1;
            }
        }
        
        //
        //  Add the server socket to things we want to watch
        //

        if(core_server_socket)
        {
            a_socket = core_server_socket->socket();
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
    
        //
        //  Handle anything incoming on the socket
        //

        a_socket = client_socket_to_mfd->socket();
        if(a_socket > 0)
        {
            if(FD_ISSET(a_socket, &readfds))
            {
                readFromMFD();
            }
        }
    }
}    

void ClientHttpServer::readFromMFD()
{
    char incoming[2049];

    int length = client_socket_to_mfd->readBlock(incoming, 2048);
    if(length > 0)
    {
        if(length >= 2048)
        {
            // oh crap
            warning("daap client socket to mfd is getting too much data");
            length = 2048;
        }
        incoming[length] = '\0';
        QString incoming_data = incoming;
        
        incoming_data.simplifyWhiteSpace();

        QStringList line_by_line = QStringList::split("\n", incoming_data);        

        for(uint i = 0; i < line_by_line.count(); i++)
        {
            QStringList tokens = QStringList::split(" ", line_by_line[i]);
            if(tokens.count() >= 7)
            {
                if(tokens[0] == "services" &&
                   tokens[2] == "lan" &&
                   tokens[3] == "mhttp")
                {
                    QString service_name = tokens[6];
                    for(uint i = 7; i < tokens.count(); i++)
                    {
                        service_name += " ";
                        service_name += tokens[i];
                    }
                    
                    if(tokens[1] == "found")
                    {
                        addMHttpServer(tokens[4], tokens[5].toUInt(), service_name);
                    }
                    else if(tokens[1] == "lost")
                    {
                        removeMHttpServer(tokens[4], tokens[5].toUInt(), service_name);
                    }
                    else
                    {
                        warning("got a services update where 2nd token was neither \"found\" nor \"lost\"");
                    }
                }
                   
            }
        }
    }
    
}

void ClientHttpServer::addMHttpServer(QString server_address, uint server_port, QString service_name)
{
    log(QString("httpserver has noticed a new mhttp service: \"%1\" (%2:%3)")
        .arg(service_name)
        .arg(server_address)
        .arg(server_port), 10);

    QString constructed_url = QString("http://%1:%2/").arg(server_address).arg(server_port);

    OtherUFPI *new_ufpi = new OtherUFPI(service_name, constructed_url);

    other_ufpi_mutex.lock();
        other_ufpi.append(new_ufpi);
    other_ufpi_mutex.unlock();
}

void ClientHttpServer::removeMHttpServer(QString server_address, uint server_port, QString service_name)
{
    log(QString("httpservice has noticed that a service went away: \"%1\" (%2:%3)")
        .arg(service_name)
        .arg(server_address)
        .arg(server_port), 10);
        
    //
    //  Find it and delete it
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
            other_ufpi.remove(which_one);
        }
        else
        {
            warning("told to delete a UFPI I wasn't aware of:"); 
        }
    other_ufpi_mutex.unlock();
}



void ClientHttpServer::handleIncoming(HttpRequest *http_request, int)
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
    
    addTopHTML(http_request);
    startCoreTable(http_request);
    listMFDs(http_request);
    listSections(http_request, branch_one);
    showCurrentSection(http_request, branch_one, branch_two);
    endCoreTable(http_request);
    addBottomHTML(http_request);

    log(QString("built HTML response for client in %1 second(s)")
        .arg(page_build_time.elapsed() / 1000.0), 9);    
    //
    //  And we're done. Our parent class takes care of sending the response
    //
}

void ClientHttpServer::addTopHTML(HttpRequest *http_request)
{
    http_request->getResponse()->addHeader("Content-Type: text/html");
    http_request->getResponse()->clearPayload();
    
    //
    //  Just building up the HTML as we go here ... pretty hacky
    //

    http_request->getResponse()->addToPayload("<html xmlns=\"http://www.w3.org/1999/xhtml\">");
    http_request->getResponse()->addToPayload("<head>");
    http_request->getResponse()->addToPayload(QString("<title>MFD on %1</title>").arg(my_hostname));
    http_request->getResponse()->addToPayload("</head>");
    http_request->getResponse()->addToPayload("<body bgcolor=\"#FFFFFF\" text=\"#000000\" link=\"#0000FF\" vlink=\"#000080\" alink=\"#FF0000\">");
    http_request->getResponse()->addToPayload("<center>");
    http_request->getResponse()->addToPayload("<p>MFD Ultra Fancy Pants Interface</p>");
    http_request->getResponse()->addToPayload("</center>");
    http_request->getResponse()->addToPayload("<hr><br><br>");
}

void ClientHttpServer::startCoreTable(HttpRequest *http_request)
{
    http_request->getResponse()->addToPayload("<center>");
    http_request->getResponse()->addToPayload("<table>");
}

void ClientHttpServer::listMFDs(HttpRequest *http_request)
{
    http_request->getResponse()->addToPayload("<tr>");
    http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
    http_request->getResponse()->addToPayload(QString("UFPI on %1").arg(mfdContext->getHostName()));

    other_ufpi_mutex.lock();
        for(uint i = 0; i < other_ufpi.count(); i++)
        {
            http_request->getResponse()->addToPayload(QString(" | <a href=\"%1\">%2</a>")
            .arg(other_ufpi.at(i)->getUrl())
            .arg(other_ufpi.at(i)->getName())
            );
        }
    other_ufpi_mutex.unlock();

    http_request->getResponse()->addToPayload("</td>");
    http_request->getResponse()->addToPayload("</tr>");

    http_request->getResponse()->addToPayload("<tr>");
    http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
    http_request->getResponse()->addToPayload("<hr>");
    http_request->getResponse()->addToPayload("</td>");
    http_request->getResponse()->addToPayload("</tr>");
}

void ClientHttpServer::listSections(HttpRequest *http_request, const QString &branch_one)
{
    http_request->getResponse()->addToPayload("<tr>");
    http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));

    //
    //  Add the various "sections", with an HREF iff it's not the section we're currently in
    //
    
    if(branch_one == "status")
    {
        http_request->getResponse()->addToPayload("Status ");
    }
    else
    {
        http_request->getResponse()->addToPayload("<a href=\"/status\">Status</a> ");
    }

    if(branch_one == "audio")
    {
        http_request->getResponse()->addToPayload("| Audio ");
    }
    else
    {
        http_request->getResponse()->addToPayload("| <a href=\"/audio/playlists\">Audio</a> ");
    }

    if(branch_one == "video")
    {
        http_request->getResponse()->addToPayload("| Video ");
    }
    else
    {
        http_request->getResponse()->addToPayload("| <a href=\"/video\">Video</a> ");
    }

    if(branch_one == "images")
    {
        http_request->getResponse()->addToPayload("| Images ");
    }
    else
    {
        http_request->getResponse()->addToPayload("| <a href=\"/images\">Images</a> ");
    }

    http_request->getResponse()->addToPayload("</td>");
    http_request->getResponse()->addToPayload("</tr>");

    http_request->getResponse()->addToPayload("<tr>");
    http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
    http_request->getResponse()->addToPayload("<hr>");
    http_request->getResponse()->addToPayload("</td>");
    http_request->getResponse()->addToPayload("</tr>");

}

void ClientHttpServer::showCurrentSection(HttpRequest *http_request, const QString &branch_one, const QString &branch_two)
{

    //
    //  Do the menu bit for this section
    //

    if(branch_one == "status" || branch_one == "video" || branch_one == "images")
    {
        http_request->getResponse()->addToPayload("<tr>");
        http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
        http_request->getResponse()->addToPayload("Doesn't | Do | Anything | Yet");
        http_request->getResponse()->addToPayload("</td>");
        http_request->getResponse()->addToPayload("</tr>");

        http_request->getResponse()->addToPayload("<tr>");
        http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
        http_request->getResponse()->addToPayload("<hr>");
        http_request->getResponse()->addToPayload("</td>");
        http_request->getResponse()->addToPayload("</tr>");
        
        http_request->getResponse()->addToPayload("<tr><td>&nbsp</td><td>&nbsp</td><td>&nbsp</td></tr>");
        http_request->getResponse()->addToPayload("<tr><td>&nbsp</td><td align=\"center\">Not Yet</td><td>&nbsp</td></tr>");
        http_request->getResponse()->addToPayload("<tr><td>&nbsp</td><td>&nbsp</td><td>&nbsp</td></tr>");
        
    }
    else if(branch_one == "audio")
    {
        if(branch_two == "playlists")
        {
            http_request->getResponse()->addToPayload("<tr>");
            http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
            http_request->getResponse()->addToPayload("Playlists | <a href=\"/audio/tracks\">Tracks</a> ");
            http_request->getResponse()->addToPayload("</td>");
            http_request->getResponse()->addToPayload("</tr>");

            http_request->getResponse()->addToPayload("<tr>");
            http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
            http_request->getResponse()->addToPayload("<hr>");
            http_request->getResponse()->addToPayload("</td>");
            http_request->getResponse()->addToPayload("</tr>");
            
            http_request->getResponse()->addToPayload("<tr>");

            http_request->getResponse()->addToPayload("<td colspan=\"3\" align=\"center\">");

            http_request->getResponse()->addToPayload(" <a href=\"/audio/playlists?stopcommand=prev\">PREV</a> ");
            http_request->getResponse()->addToPayload(" &nbsp; &nbsp; <a href=\"/audio/playlists?stopcommand=stop\">STOP</a> ");
            http_request->getResponse()->addToPayload(" &nbsp; &nbsp; <a href=\"/audio/playlists?stopcommand=next\">NEXT</a> ");

            http_request->getResponse()->addToPayload("</td>");

            http_request->getResponse()->addToPayload("</tr>");

            showPlaylists(http_request);
        }
        else if(branch_two == "tracks")
        {
            http_request->getResponse()->addToPayload("<tr>");
            http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
            http_request->getResponse()->addToPayload("<a href=\"/audio/playlists\">Playlists</a> | Tracks ");
            http_request->getResponse()->addToPayload("</td>");
            http_request->getResponse()->addToPayload("</tr>");

            http_request->getResponse()->addToPayload("<tr>");
            http_request->getResponse()->addToPayload(QString("<td colspan=\"%1\" align=\"left\">").arg(core_table_columns));
            http_request->getResponse()->addToPayload("<hr>");
            http_request->getResponse()->addToPayload("</td>");
            http_request->getResponse()->addToPayload("</tr>");

            http_request->getResponse()->addToPayload("<tr>");
            http_request->getResponse()->addToPayload("<td colspan=\"3\" align=\"center\"><a href=\"/audio/tracks?stopcommand=stop\">STOP</a></td>");
            http_request->getResponse()->addToPayload("</td>");
            http_request->getResponse()->addToPayload("</tr>");



            showTracks(http_request);

        }
        else
        {
            http_request->getResponse()->setError(404);
        }
    }
    else
    {
        http_request->getResponse()->setError(404);
    }

}

void ClientHttpServer::showPlaylists(HttpRequest *http_request)
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
                        http_request->getResponse()->addToPayload("<tr bgcolor=\"ccccee\">");
                    }
                    else
                    {
                        http_request->getResponse()->addToPayload("<tr>");
                    }
                    ++counter;

                    http_request->getResponse()->addToPayload("<td>");
                    http_request->getResponse()->addToPayload(a_playlist->getName());
                    http_request->getResponse()->addToPayload("</td>");

                    http_request->getResponse()->addToPayload("<td>");
                    http_request->getResponse()->addToPayload("</td>");

                    http_request->getResponse()->addToPayload("<td>");
                    http_request->getResponse()->addToPayload(
                        QString("<a href=\"/audio/playlists?playplaylist=%1&container=%2\">play</a>")
                        .arg(a_playlist->getId())
                        .arg(a_container->getIdentifier()));
                    http_request->getResponse()->addToPayload("</td>");

                    http_request->getResponse()->addToPayload("</tr>");
                }
            }
        }
    }
    
    //
    //  Unlock the metadata
    //    

    metadata_server->unlockMetadata();

}

void ClientHttpServer::showTracks(HttpRequest *http_request)
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
                        AudioMetadata *which_item = (AudioMetadata*)iterator.current();
                    
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
                                http_request->getResponse()->addToPayload("<tr bgcolor=\"ccccee\">");
                            }
                            else
                            {
                                http_request->getResponse()->addToPayload("<tr>");
                            }
                            ++counter;

                            http_request->getResponse()->addToPayload("<td>");
                            http_request->getResponse()->addToPayload(which_item->getArtist());
                            http_request->getResponse()->addToPayload("</td>");

                            http_request->getResponse()->addToPayload("<td>");
                            http_request->getResponse()->addToPayload(which_item->getTitle());
                            http_request->getResponse()->addToPayload("</td>");

                            http_request->getResponse()->addToPayload("<td>");
                            http_request->getResponse()->addToPayload(
                                QString("<a href=\"/audio/tracks?playtrack=%1&container=%2\">play</a>")
                                .arg(which_item->getId())
                                .arg(a_container->getIdentifier()));
                            http_request->getResponse()->addToPayload("</td>");

                            http_request->getResponse()->addToPayload("</tr>");
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

void ClientHttpServer::endCoreTable(HttpRequest *http_request)
{
    http_request->getResponse()->addToPayload("</table>");
    http_request->getResponse()->addToPayload("</center>");
}

void ClientHttpServer::addBottomHTML(HttpRequest *http_request)
{
    http_request->getResponse()->addToPayload("<br><br><hr>");
    http_request->getResponse()->addToPayload("<font size=\"-2\">");
    http_request->getResponse()->addToPayload("<center>");
    http_request->getResponse()->addToPayload("<p>&copy 2002, 2003, 2004 &nbsp by Isaac Richards, Thor Sigvaldason, and all the <a href=\"http://www.mythtv.org/\">MythTV</a> contributors.</p>");
    http_request->getResponse()->addToPayload("</center>");
    http_request->getResponse()->addToPayload("</font>");
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

