/*
	mfdinstance.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	You get one of these objects for every mfd in existence

*/

#include <unistd.h>
#include <iostream>
using namespace std;

#include "mfdinstance.h"
#include "audioclient.h"
#include "metadataclient.h"

MfdInstance::MfdInstance(
                            const QString &l_hostname,
                            const QString &l_ip_address,
                            int l_port
                        )
{
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

    QHostAddress this_address;

    if(!this_address.setAddress(ip_address))
    {
        cerr << "mfdinstance.o: can't set address to mfd" << endl;
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
                            
            return;
        }
        msleep(300);
    }

    QString first_and_only_command = "services list\n\r";
    if(client_socket_to_mfd->writeBlock(
                                        first_and_only_command.ascii(), 
                                        first_and_only_command.length()
                                       )
       != (int) first_and_only_command.length())
    {
        cerr << "mfdinstance.o: error writing to my mfd, "
             << "giving up"
             << endl;
        return;
    }

    while(keep_going)
    {
    
        fd_set fds;
        int nfds = 0;
        FD_ZERO(&fds);

        //
        //  Watch out main socket to the mfd
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
        //  Sleep as long as mdnsd tells us to
        //

        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        
        
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
            else if(tokens[3] == "mmdp")
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
        }
    }    
}

void MfdInstance::addAudioClient(const QString &address, uint a_port)
{
    AudioClient *new_audio = new AudioClient(address, a_port);
    if(new_audio->connect())
    {
        my_service_clients->append(new_audio);
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
    MetadataClient *new_metadata = new MetadataClient(address, a_port);
    if(new_metadata->connect())
    {
        my_service_clients->append(new_metadata);
    }
    else
    {
        cerr << "mfdinstance.o: tried to create a metadata client, "
             << "but could not connect"
             << endl;
        delete new_metadata;
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

