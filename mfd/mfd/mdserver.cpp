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
    file_descriptors_mutex = new QMutex();
    file_descriptors = new IntValueList();
};

void MetadataServer::makeMeDoSomething(const QString& what_to_do)
{
    things_to_do_mutex.lock();
        SocketBuffer *new_request = new SocketBuffer(what_to_do, -1);
        things_to_do.append(new_request);    
    things_to_do_mutex.unlock();
    main_wait_condition.wakeAll();
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
    
    int fd_watching_pipe[2];
    if(pipe(fd_watching_pipe) < 0)
    {
        warning("metadata server could not create a pipe to its FD watching thread");
    }


    fd_watcher = new MFDFileDescriptorWatchingPlugin(
                                                        this, 
                                                        &file_watching_mutex, 
                                                        file_descriptors_mutex, 
                                                        file_descriptors, 
                                                        30,
                                                        0);
    file_watching_mutex.lock();
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
            //
            //  List descriptors that should be watched in the file descriptor watching thread/object
            //

            file_descriptors_mutex->lock();
                
                //
                //  Zero 'em out.
                //

                file_descriptors->clear();
                
                //
                //  Add the server socket
                //

                file_descriptors->append(core_server_socket->socket());
            
                //
                //  Next, add all the client sockets
                //
            
                QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
                MFDServiceClientSocket *a_client;
                while ( (a_client = iterator.current()) != 0 )
                {
                    ++iterator;
                    file_descriptors->append(a_client->socket());
                }
            
    		    
    		    //
    		    //  Finally, add the read side of the control pipe
    		    //
    		    
    		    file_descriptors->append(fd_watching_pipe[0]);

            file_descriptors_mutex->unlock();

 
 
            //
            //  Let the descriptor watching thread run to it's select() call
            //

            file_watching_mutex.unlock();


            //
            //  Wait for something to happen
            //

            main_wait_condition.wait();


            //
            //  In case it was not the descriptor watcher that woke us up,
            //  make it give up it's lock on the watching mutex.
            //

            write(fd_watching_pipe[1], "X", 1);
            file_watching_mutex.lock();
            char back[3];
            read(fd_watching_pipe[0], back, 2);
        }
    }

    //
    //  Stop the sub thread
    //
    
    fd_watcher->stop();
    file_watching_mutex.unlock();
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
    return true;
}

MetadataServer::~MetadataServer()
{
    if(fd_watcher)
    {
        delete fd_watcher;
    }
    if(file_descriptors)
    {
        if(file_descriptors_mutex)
        {
            file_descriptors_mutex->lock();
            file_descriptors->clear();
            delete file_descriptors;
            file_descriptors = NULL;
            file_descriptors_mutex->unlock();
        }
        else
        {
            file_descriptors->clear();
            delete file_descriptors;
            file_descriptors = NULL;
        }
    }

    if(file_descriptors_mutex)
    {
        delete file_descriptors_mutex;
        file_descriptors_mutex = NULL;
    }
}
