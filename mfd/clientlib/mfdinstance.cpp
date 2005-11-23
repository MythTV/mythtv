/*
	mfdinstance.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	You get one of these objects for every mfd in existence

*/

#include <unistd.h>
#include <iostream>
using namespace std;

#include <qapplication.h>

#include "mfdinterface.h"
#include "mfdinstance.h"
#include "audioclient.h"
#include "metadataclient.h"
#include "rtspclient.h"
#include "transcoderclient.h"
#include "events.h"

MfdInstance::MfdInstance(
                            int an_mfd,
                            MfdInterface *the_interface,
                            const QString &l_name,
                            const QString &l_hostname,
                            const QString &l_ip_address,
                            int l_port
                        )
{
    mfd_id = an_mfd;
    mfd_interface = the_interface;
    name = l_name;
    hostname= l_hostname;
    ip_address = l_ip_address;
    port = l_port;
    keep_going = true;

    //
    //  Create a u shaped pipe so others can wake us out of a select
    //
    

    if(pipe(u_shaped_pipe) < 0)
    {
        warning("could not create a u shaped pipe");
    }

    //
    //  We open the socket during run()
    //    

    client_socket_to_mfd = NULL;

    //
    //  We begin life with no clients to mfd services
    //
    
    my_service_clients = new QPtrList<ServiceClient>;
    my_service_clients->setAutoDelete(true);
    
}

void MfdInstance::run()
{
    //
    //  set our priority
    //
    
    //int nice_level = mfdContext->getNumSetting("clientlib-mfdinstane-nice", 18);
    int nice_level = 18;
    nice(nice_level);

    QHostAddress this_address;

    if(!this_address.setAddress(ip_address))
    {
        cerr << "mfdinstance.o: can't set address to mfd" << endl;
        announceMyDemise();
        return;
    }

    client_socket_to_mfd = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_mfd->setBlocking(false);
    
    int connect_tries = 0;
    
    while(! (client_socket_to_mfd->connect(this_address, port)))
    {
        //
        //  We give this a few attempts. It can take a few on non-blocking sockets.
        //

        ++connect_tries;

        if(connect_tries > 10)
        {
            cerr << "mfdinstance.o: could not connect to mfd on \""
                 << hostname
                 << "\""
                 << endl;
                  
            announceMyDemise();          
            return;
        }
        msleep(300);
    }

    QString first_and_only_command = "services list\n\r";


    //
    //  We try up to 10 times to write our only message. If that fails, we bail.
    //


    int write_tries = 0;
    while(
            client_socket_to_mfd->writeBlock(
                                                first_and_only_command.ascii(),
                                                first_and_only_command.length()
                                            )
            < 0)
    {
        ++write_tries;

        if(write_tries > 10)
        {
            cerr << "mfdinstance.o: tried to write "
                 << first_and_only_command.length()
                 << " bytes to mfd at "
                 << ip_address
                 << ":"
                 << port
                 << ", but failed repeatedly and is giving up "
                 << endl;
        
            announceMyDemise();
            return;
        }
        msleep(300);
    }

    while(keep_going)
    {

        //
        //  See if we have any pending commands
        //  
        
        QStringList new_command;
        pending_commands_mutex.lock();
            if(pending_commands.count() > 0)
            {
                new_command = pending_commands[0];
                pending_commands.pop_front();
            }
        pending_commands_mutex.unlock();

        if(new_command.count() > 0)
        {
            executeCommands(new_command);
        }
    
        //
        //  Setup a list of descriptors to watch for any activity
        //

        fd_set fds;
        int nfds = 0;
        FD_ZERO(&fds);

        //
        //  Watch our main socket to the mfd
        //
        
        if(client_socket_to_mfd)
        {
            int a_socket = client_socket_to_mfd->socket();
            if(a_socket > 0)
            {
                FD_SET(a_socket, &fds);
                if(nfds <= a_socket)
                {
                    nfds = a_socket + 1;
                }
            }
        }
        
        //
        //  Watch all of our services for incoming
        //
        
        for(
            ServiceClient *an_sc = my_service_clients->first();
            an_sc;
            an_sc = my_service_clients->next()
           )
        {
            int a_socket = an_sc->getSocket();
            if(a_socket > 0)
            {
                FD_SET(a_socket, &fds);
                if(nfds <= a_socket)
                {
                    nfds = a_socket + 1;
                }
            }
        }

        //
        //  Add the control pipe
        //
            
        FD_SET(u_shaped_pipe[0], &fds);
        if(nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    
    
        //
        //  Sleep for up to 10 seconds
        //

        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        pending_commands_mutex.lock();
            if(pending_commands.count() > 0)
            {
                timeout.tv_sec = 0;
            }
        pending_commands_mutex.unlock();        
        
        //
        //  Sit in select until something happens
        //
        
        int result = select(nfds,&fds,NULL,NULL,&timeout);
        if(result < 0)
        {
            cerr << "mfdinstance.o: got an error on select (?), "
                 << "which is far from a good thing" 
                 << endl;
        }
    
        //
        //  In case data came in on out u_shaped_pipe, clean it out
        //

        if(FD_ISSET(u_shaped_pipe[0], &fds))
        {
            u_shaped_pipe_mutex.lock();
                char read_back[2049];
                read(u_shaped_pipe[0], read_back, 2048);
            u_shaped_pipe_mutex.unlock();
        }
        
        //
        //  See if we have anything coming in the core mfd socket
        //
        
        if(client_socket_to_mfd)
        {
            int bytes_available = client_socket_to_mfd->bytesAvailable();
            if(bytes_available > 0)
            {
                readFromMfd();
            }
        }
        
        //
        //  See if anything came in from the services
        //

        for(
            ServiceClient *an_sc = my_service_clients->first();
            an_sc;
            an_sc = my_service_clients->next()
           )
        {
            if(an_sc->bytesAvailable() > 0)
            {
                an_sc->handleIncoming();
            }
        }
    }
}

void MfdInstance::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
    wakeUp();
}

void MfdInstance::wakeUp()
{
    //
    //  Tell the main thread to wake up by sending some data to ourselves on
    //  our u_shaped_pipe. This may seem odd. It isn't.
    //

    u_shaped_pipe_mutex.lock();
        write(u_shaped_pipe[1], "wakeup\0", 7);
    u_shaped_pipe_mutex.unlock();
    
}

void MfdInstance::readFromMfd()
{
    char in_buffer[2049];
    
    int amount_read = client_socket_to_mfd->readBlock(in_buffer, 2048);
    if(amount_read < 0)
    {
        cerr << "mfdinstance.o: error reading mfd socket" << endl;
        return;
    }
    else if(amount_read == 0)
    {
        return;
    }
    else
    {
        in_buffer[amount_read] = '\0';
        QString incoming_text = QString(in_buffer);

        incoming_text.simplifyWhiteSpace();

        QStringList line_by_line = QStringList::split("\n", incoming_text);

        for(uint i = 0; i < line_by_line.count(); i++)
        {
            QStringList tokens = QStringList::split(" ", line_by_line[i]);
            parseFromMfd(tokens);
        }
    }
}

void MfdInstance::parseFromMfd(QStringList &tokens)
{
    //
    //  We pick out anything that has to do with services, and then look for
    //  ones we care about (ie. those running on "host" and those we
    //  understand how to interact with)
    //

    if(tokens.count() >= 7)
    {
        if(tokens[0] == "services" &&
           tokens[2] == "host")
        {
            if(tokens[3] == "macp")
            {
                if(tokens[1] == "found")
                {
                    //
                    //  Add myth audio control
                    //
                    
                    addAudioClient(tokens[4], tokens[5].toUInt());
                    
                }
                else if(tokens[1] == "lost")
                {
                    //
                    //  Remove myth audio control
                    //
                    
                    removeServiceClient(
                                        MFD_SERVICE_AUDIO_CONTROL,
                                        tokens[4], 
                                        tokens[5].toUInt()
                                       );
                    
                }
                else
                {
                    cerr << "mfdinstance.o: got service announcement where "
                         << "2nd token was neither \"found\" nor \"lost\""
                         << endl;
                }
            }
            else if(tokens[3] == "mdcap")
            {
                if(tokens[1] == "found")
                {
                    //
                    //  Add metadata stuff
                    //

                    addMetadataClient(tokens[4], tokens[5].toUInt());
                }
                else if(tokens[1] == "lost")
                {
                    //
                    //  Remove metadata stuff
                    //

                    removeServiceClient(
                                        MFD_SERVICE_METADATA,
                                        tokens[4], 
                                        tokens[5].toUInt()
                                       );
                }
                else
                {
                    cerr << "mfdinstance.o: got metadata service announcement "
                         << "where 2nd token was neither \""
                         << "found\" nor \"lost\""
                         << endl;
                }
            }
            else if(tokens[3] == "rtsp")
            {
                if(tokens[1] == "found")
                {
                    //
                    //  Add rtsp client (to do music visualizations)
                    //
                    addRtspClient(tokens[4], tokens[5].toUInt());
                }
                else if(tokens[1] == "lost")
                {
                    //
                    //  Remove rtsp client
                    //
                    removeServiceClient(
                                        MFD_SERVICE_RTSP_CLIENT,
                                        tokens[4],
                                        tokens[5].toUInt()
                                       );
                }
                else
                {
                    cerr << "mfdinstance.o: got rtsp announcement "
                         << "where 2nd token was neither \""
                         << "found\" nor \"lost\""
                         << endl;
                }
            }
            else if(tokens[3] == "mtcp")
            {
                if(tokens[1] == "found")
                {
                    //
                    //  Add transcoder client (to rip stuff, see progress)
                    //
                    addMtcpClient(tokens[4], tokens[5].toUInt());
                }
                else if(tokens[1] == "lost")
                {
                    //
                    //  Remove transcoder client
                    //
                    removeServiceClient(
                                        MFD_SERVICE_TRANSCODER,
                                        tokens[4],
                                        tokens[5].toUInt()
                                       );
                }
                else
                {
                    cerr << "mfdinstance.o: got mtcp announcement "
                         << "where 2nd token was neither \""
                         << "found\" nor \"lost\""
                         << endl;
                }
            }
        }
    }    
}

void MfdInstance::addAudioClient(const QString &address, uint a_port)
{
    AudioClient *new_audio = new AudioClient(mfd_interface, mfd_id, address, a_port);
    if(new_audio->connect())
    {
        my_service_clients->append(new_audio);
        
        //
        //  Sound out an event to let this library's main thread send a
        //  signal that an audio plugin exists on this mfd
        //
        
        MfdAudioPluginExistsEvent *apee = new MfdAudioPluginExistsEvent(mfd_id);
        QApplication::postEvent(mfd_interface, apee);
        
    }
    else
    {
        cerr << "mfdinstance.o: tried to create an audio client, "
             << "but could not connect"
             << endl;
        delete new_audio;
    }
}

void MfdInstance::addMetadataClient(const QString &address, uint a_port)
{
    MetadataClient *new_metadata = new MetadataClient(mfd_interface, mfd_id, address, a_port, this);
    if(new_metadata->connect())
    {
        my_service_clients->append(new_metadata);
        new_metadata->sendFirstRequest();
    }
    else
    {
        cerr << "mfdinstance.o: tried to create a metadata client, "
             << "but could not connect"
             << endl;
        delete new_metadata;
    }
}

void MfdInstance::addRtspClient(const QString &address, uint a_port)
{
/*

    RtspClient *new_rtsp = new RtspClient(mfd_interface, mfd_id, address, a_port);
    if(new_rtsp->connect())
    {
        my_service_clients->append(new_rtsp);
    }
    else
    {
        cerr << "mfdinstance.o: tried to create an rtsp client, "
             << "but could not connect"
             << endl;
        delete new_rtsp;
    }
*/
}

void MfdInstance::addMtcpClient(const QString &address, uint a_port)
{
    TranscoderClient *new_transcoder = new TranscoderClient(mfd_interface, mfd_id, address, a_port);
    if(new_transcoder->connect())
    {
        my_service_clients->append(new_transcoder);
        new_transcoder->sendFirstRequest();
        MfdTranscoderPluginExistsEvent *tpee = new MfdTranscoderPluginExistsEvent(mfd_id);
        QApplication::postEvent(mfd_interface, tpee);
    }
    else
    {
        cerr << "mfdinstance.o: tried to create a transcoder (mtcp) client, "
             << "but could not connect"
             << endl;
        delete new_transcoder;
    }
}

void MfdInstance::removeServiceClient(
                                        MfdServiceType type,
                                        const QString &address, 
                                        uint a_port
                                     )
{
    //
    //  Find the service client to remove
    //

    ServiceClient *which_one = NULL;
    for(
        ServiceClient *an_sc = my_service_clients->first();
        an_sc;
        an_sc = my_service_clients->next()
       )
    {
        if(
            an_sc->getType()    == type      &&
            an_sc->getAddress() == address   &&
            an_sc->getPort()    == a_port
          )
        {
            which_one = an_sc;
            break;
        }
    }
    
    //
    //  It it's there, remove it
    //
    
    if(which_one)
    {
        my_service_clients->remove(which_one);
    }
    else
    {
        cerr << "mfdinstance.o: asked to remove a service client that "
             << "doesn't exist"
             << endl;
    }
}

void MfdInstance::announceMyDemise()
{
    MfdDiscoveryEvent *de = new MfdDiscoveryEvent(
                                                    false, 
                                                    getName(),
                                                    getHostname(),
                                                    getAddress(),
                                                    getPort()
                                                 );

    QApplication::postEvent( mfd_interface, de );
}

void MfdInstance::commitListEdits(
                                    UIListGenericTree *playlist_tree,
                                    bool new_playlist,
                                    QString playlist_name
                                 )
{
    //
    //  Find the metadata client object that will deal with this
    //
    
    bool found_one = false;
    for(
            ServiceClient *an_sc = my_service_clients->first();
            an_sc;
            an_sc = my_service_clients->next()
           )
    {
        if(an_sc->getMagicToken() == "metadata")
        {
            if ( MetadataClient *a_metadata_sc = dynamic_cast<MetadataClient*>(an_sc) )
            {
                found_one = true;
                a_metadata_sc->commitListEdits(
                                                playlist_tree,
                                                new_playlist,
                                                playlist_name
                                              );
                break;
            }
        }
    }

    if(!found_one)
    {
        cerr << "mfdinstance.o: found no metadata service "
             << "client to hand off a commitEdits() to "
             << endl;
        return;
    }    
}

void MfdInstance::addPendingCommand(QStringList new_command)
{
    if(new_command.count() < 1)
    {
        cerr << "mfdinstance.o: something is trying to "
             << "addPendingCommand(), but passed no commands "
             << endl;
        return;
    }
    if(new_command.count() < 2)
    {
        cerr << "mfdinstance.o: something is trying to "
             << "addPendingCommand(), but only passed a magic "
             << "token of \""
             << new_command[0] 
             << "\" with no actual commands."
             << endl;
        return;
    }
    
    pending_commands_mutex.lock();
        pending_commands.append(new_command);
    pending_commands_mutex.unlock();
    wakeUp();
}

void MfdInstance::executeCommands(QStringList new_command)
{
    //
    //  Make sure the list of tokens in new_command contains _at least_ a
    //  magic token (e.g. "audio") and some actual command
    //
    
    if(new_command.count() < 2)
    {
        cerr << "mfdinstance.o: asked to executeCommands() with"
             << "no commands"
             << endl;
             return;
    }
    
    //
    //  Pop off the magic token
    //

    QString magic_token = new_command[0];
    new_command.pop_front();
    
    //
    //  Find the service client object that will deal with these kinds of
    //  commands
    //
    
    bool found_one = false;
    for(
            ServiceClient *an_sc = my_service_clients->first();
            an_sc;
            an_sc = my_service_clients->next()
           )
    {
        if(an_sc->getMagicToken() == magic_token)
        {
            found_one = true;
            an_sc->executeCommand(new_command);
        }
    }
    
    if(!found_one)
    {
        cerr << "mfdinstance.o: no service client object would "
             << "deal with a magic_token of \""
             << magic_token
             << "\""
             << endl;
    }
}

bool MfdInstance::hasTranscoder()
{
    bool return_value = false;

    //
    //  See if we can find a transcoder
    //

    for(
        ServiceClient *an_sc = my_service_clients->first();
        an_sc;
        an_sc = my_service_clients->next()
       )
    {
        if(an_sc->getType()    == MFD_SERVICE_TRANSCODER)
        {
            return_value = true;
            break;
        }
    }
    
    return return_value;
}

MfdInstance::~MfdInstance()    
{
    if(client_socket_to_mfd)
    {
        delete client_socket_to_mfd;
        client_socket_to_mfd = NULL;
    }
    
    if(my_service_clients)
    {
        my_service_clients->clear();
        delete my_service_clients;
        my_service_clients = NULL;
    }
}

