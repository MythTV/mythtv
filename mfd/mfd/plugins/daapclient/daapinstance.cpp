/*
    daapinstance.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    An object that knows how to talk to daap servers

*/

#include "daapclient.h"
#include "daaprequest.h"
#include "daapresponse.h"

DaapInstance::DaapInstance(
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
    
    parent = owner;
    server_address = l_server_address;
    server_port = l_server_port;
    service_name = l_service_name;
    keep_going = true;
    client_socket_to_daap_server = NULL;
    if(pipe(u_shaped_pipe) < 0)
    {
        warning("daap instance could not create a u shaped pipe");
    }
    
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
        warning(QString("daap instance could not "
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
            warning(QString("daap instance could not "
                            "connect to server %1:%2")
                            .arg(server_address)
                            .arg(server_port));
            return;
        }
        usleep(100);
    }

    log(QString("daap instance made basic "
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
        //  Listen for the server socket
        //

        FD_ZERO(&readfds);
        FD_SET(client_socket_to_daap_server->socket(), &readfds);
        if(nfds <= client_socket_to_daap_server->socket())
        {
            nfds = client_socket_to_daap_server->socket() + 1;
        }
        
        //
        //  Listen on our u-shaped pipe
        //

        FD_SET(u_shaped_pipe[0], &readfds);
        if(nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    

        
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        int result = select(nfds, &readfds, NULL, NULL, &timeout);
        if(result < 0)
        {
            warning("daap instance got an error from select() "
                    "... not sure what to do");
        }
        else
        {
            if(FD_ISSET(client_socket_to_daap_server->socket(), &readfds))
            {
                //
                //  Excellent ... the daap server is sending us some data.
                //
                
                handleIncoming();
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
    //  Kill off the socket
    //
    
    delete client_socket_to_daap_server;
    client_socket_to_daap_server = NULL;
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
    //  This just makes a daapresponse object out of the raw incoming socket
    //  data. NB: assume all the response is here ... probably need to FIX
    //  that at some point.
    //

    
    
    //
    //  Collect the incoming data
    //  
    
    char incoming[10000];   // FIX this crap soon
    int length = 0;
    
    length = client_socket_to_daap_server->readBlock(incoming, 10000 - 1);
    if(length > 0)
    {
    
        DaapResponse *new_response = new DaapResponse(this, incoming, length);
        processResponse(new_response);
        delete new_response;
    }
    
}

void DaapInstance::processResponse(DaapResponse *daap_response)
{
    daap_response = daap_response;
}

DaapInstance::~DaapInstance()
{
}

