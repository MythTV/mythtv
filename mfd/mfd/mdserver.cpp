/*
	mdserver.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Implementation of a threaded object that opens
	a server port and explains the state of the 
	metadata to any client that asks

*/

#include <iostream>
using namespace std;

#include "mdserver.h"
#include "../mfdlib/mfd_events.h"

MetadataServer::MetadataServer(MFD* owner, int port)
               :MFDServicePlugin(owner, -1, port)
{

};

void MetadataServer::makeMeDoSomething(const QString& what_to_do)
{
    things_to_do_mutex.lock();
        SocketBuffer *new_request = new SocketBuffer(what_to_do, -1);
        things_to_do.append(new_request);    
    things_to_do_mutex.unlock();
}

void MetadataServer::run()
{
    //
    //  Build a hostname so we can advertise ourselves to the world (via
    //  zeroconfig) as an mmdp (Myth Meta Data Protocol) service
    //

    char my_hostname[2049];
    QString local_hostname = "unknown";

    if(gethostname(my_hostname, 2048) < 0)
    {
        warning("metadata server could not call gethostname()");
    }
    else
    {
        local_hostname = my_hostname;
    }

    ServiceEvent *se = new ServiceEvent(QString("services add mmdp %1 Myth Metadata Services on %2")
                                       .arg(port_number)
                                       .arg(local_hostname));
    QApplication::postEvent(parent, se);


    //
    //  Init our sockets (note, these are low-level Socket Devices, which
    //  are thread safe)
    //
    
    if( !initServerSocket())
    {
        fatal("metadata server could not initialize its core server socket");
        return;
    }
    
    int nfds = 0;
	fd_set readfds;
    int fd_watching_pipe[2];
    if(pipe(fd_watching_pipe) < 0)
    {
        warning("metadata server could not create a pipe to its FD watching thread");
    }


    fd_watcher = new MFDFileDescriptorWatchingPlugin(this, &file_descriptors_mutex, &nfds, &readfds);
    file_descriptors_mutex.lock();
    fd_watcher->start();


    while(keep_going)
    {
        //
        //  Update the status of our sockets.
        //
        
        updateSockets();
        
        
        QStringList pending_tokens;
        int         pending_socket = 0;

        //
        //  Pull off the first pending command request
        //

        
        things_to_do_mutex.lock();
            if(things_to_do.count() > 0)
            {
                pending_tokens = things_to_do.getFirst()->getTokens();
                pending_socket = things_to_do.getFirst()->getSocketIdentifier();
                things_to_do.removeFirst();
            }
        things_to_do_mutex.unlock();
        
                    
        if(pending_tokens.count() > 0)
        {
            doSomething(pending_tokens, pending_socket);
        }
        else
        {
    		nfds = 0;
	    	FD_ZERO(&readfds);

            //
            //  Add out core server socket to the set of file descriptors to
            //  listen to
            //
            
            FD_SET(core_server_socket->socket(), &readfds);
            if(nfds <= core_server_socket->socket())
            {
                nfds = core_server_socket->socket() + 1;
            }

            //
            //  Next, add all the client sockets
            //
            
            QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
            MFDServiceClientSocket *a_client;
            while ( (a_client = iterator.current()) != 0 )
            {
                ++iterator;
                FD_SET(a_client->socket(), &readfds);
                if(nfds <= a_client->socket())
                {
                    nfds = a_client->socket() + 1;
                }
            }
            
            
            //
            //  Finally, add the control pipe
            //
            
    		FD_SET(fd_watching_pipe[0], &readfds);
            if(nfds <= fd_watching_pipe[0])
            {
                nfds = fd_watching_pipe[0] + 1;
            }
            
            
            
       		fd_watcher->setTimeout(10, 0);
            file_descriptors_mutex.unlock();

            main_wait_condition.wait();

            write(fd_watching_pipe[1], "X", 1);
            file_descriptors_mutex.lock();
            char back[2];
            read(fd_watching_pipe[0], back, 2);

        }
    }

    //
    //  Stop the sub thread
    //
    
    file_descriptors_mutex.unlock();
    fd_watcher->stop();
    fd_watcher->wait();
    delete fd_watcher;
    fd_watcher = NULL;


}

void MetadataServer::doSomething(const QStringList &tokens, int socket_identifier)
{
    bool ok = false;

    if(tokens.count() < 1)
    {
        ok = false;
    }
    else if(tokens[0] == "check" && tokens.count() == 1)
    {
        ok = true;
        checkMetadata();
    }

    if(!ok)
    {
        warning(QString("metadata server did not understand these tokens: %1").arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
    }
}

bool MetadataServer::checkMetadata()
{
//    new_overall_generation = parent
}