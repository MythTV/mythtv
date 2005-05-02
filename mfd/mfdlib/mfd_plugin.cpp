/*
	mfd_plugin.cpp

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for plugin skeleton

*/

#include <iostream>
using namespace std;

#include <qobject.h>
#include <qregexp.h>

#include "mfd_plugin.h"
#include "mfd_events.h"
#include "httpoutresponse.h"


MFDBasePlugin::MFDBasePlugin(MFD *owner, int identifier, const QString &a_name)
{
    parent = owner;
    unique_identifier = identifier;
    keep_going = true;
    name = a_name;
    services_changed_flag = false;
    metadata_collection_last_changed = 1;
}


void MFDBasePlugin::log(const QString &log_message, int verbosity)
{
    QString named_message = QString("%1 plugin %2")
                            .arg(name)
                            .arg(log_message);
    log_mutex.lock();
        if(parent)
        {
            LoggingEvent *le = new LoggingEvent(named_message, verbosity);
            QApplication::postEvent(parent, le);    
        }
    log_mutex.unlock();
}

void MFDBasePlugin::warning(const QString &warning_message)
{
    warning_mutex.lock();
        QString warn_string = QString("WARNING: %1 plugin %2")
                              .arg(name)
                              .arg(warning_message);
        log_mutex.lock();
            if(parent)
            {
                LoggingEvent *le = new LoggingEvent(warn_string, 1);
                QApplication::postEvent(parent, le);    
            }
        log_mutex.unlock();
    warning_mutex.unlock();
}

void MFDBasePlugin::fatal(const QString &death_rattle)
{
    fatal_mutex.lock();
        if(parent)
        {
            FatalEvent *fe = new FatalEvent(death_rattle, unique_identifier);
            QApplication::postEvent(parent, fe);    
        }
        else
        {
            cerr << "This is really bad. I want to tell the mfd that I'm about to die, but I don't have a pointer to it." << endl;
        }
    fatal_mutex.unlock();
}

void MFDBasePlugin::sendMessage(const QString &message, int socket_identifier)
{
    socket_mutex.lock();
        if(parent)
        {
            SocketEvent *se = new SocketEvent(message, socket_identifier);
            QApplication::postEvent(parent, se);    
        }
    socket_mutex.unlock();
}

void MFDBasePlugin::sendMessage(const QString &message)
{
    allclient_mutex.lock();
        if(parent)
        {
            AllClientEvent* ace = new AllClientEvent(message);
            QApplication::postEvent(parent, ace);    
        }
    allclient_mutex.unlock();
}

void MFDBasePlugin::huh(const QStringList &tokens, int socket_identifier)
{
    QString new_message = "huh ";
    new_message.append(tokens.join(" "));
    sendMessage(new_message, socket_identifier);
}

void MFDBasePlugin::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
    main_wait_condition.wakeAll();
}

void MFDBasePlugin::wakeUp()
{
    main_wait_mutex.lock();
        main_wait_condition.wakeAll();
    main_wait_mutex.unlock();
}

bool MFDBasePlugin::keepGoing()
{
    bool return_value;
    keep_going_mutex.lock();
        return_value = keep_going;
    keep_going_mutex.unlock();
    
    return return_value;
}

void MFDBasePlugin::metadataChanged(int which_collection, bool external)
{
    //
    //  Arbitrary limit, may need adjustment in some contexts (?).
    //
    
    uint max_metadata_event_queue_size = 100;

    metadata_changed_mutex.lock();
        MetadataEvent an_event(which_collection, external);
        metadata_events.push_back(an_event);
        if (metadata_events.size() > max_metadata_event_queue_size)
        {
            warning(QString("more than %1 unprocessed metadata events on the queue, "
                            "throuwing away oldest")
                           .arg(max_metadata_event_queue_size));
            metadata_events.pop_front();
        }
        metadata_collection_last_changed = which_collection;
    metadata_changed_mutex.unlock();

    wakeUp();
}

void MFDBasePlugin::servicesChanged()
{
    services_changed_mutex.lock();
        services_changed_flag = true;
    services_changed_mutex.unlock();
    wakeUp();
}

MFDBasePlugin::~MFDBasePlugin()
{
}

/*
---------------------------------------------------------------------
*/

MFDCapabilityPlugin::MFDCapabilityPlugin(MFD* owner, int identifier, const QString &a_name)
                    :MFDBasePlugin(owner, identifier, a_name)
{
    things_to_do.clear();
    things_to_do.setAutoDelete(true);
}

//
//  doSomething() is called (running in a separate thread) each
//  time a command that begins with one of the strings listed in 
//  my_capabilities is sent by a client. If you're doing something
//  really long, you should check keep_going (a bool) occassionaly
//  to make sure you aren't holding up a shutdown.
//

void MFDCapabilityPlugin::doSomething(const QStringList &, int)
{
    warning("Base mfdplugin's class doSomething() method was called.");
}




void MFDCapabilityPlugin::parseTokens(const QStringList &tokens, int socket_identifier)
{
    //
    //  NB: This method does not and should not get called from
    //  the separate thread. It should not block.
    //
        
    SocketBuffer *sb = new SocketBuffer(tokens, socket_identifier);
    things_to_do_mutex.lock();
        things_to_do.append(sb);
        if(things_to_do.count() > 99)
        {
            warning(QString("with unique identifier of %1 has more than %2 pending commands")
                    .arg(unique_identifier)
                    .arg(things_to_do.count()));
        }
    things_to_do_mutex.unlock();
    main_wait_condition.wakeAll();
}

QStringList MFDCapabilityPlugin::getCapabilities()
{
    return my_capabilities;
}

void MFDCapabilityPlugin::run()
{
    runOnce();
    while(keep_going)
    {
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
            main_wait_condition.wait();
        }
    }
}

MFDCapabilityPlugin::~MFDCapabilityPlugin()
{
}

/*
---------------------------------------------------------------------
*/

MFDServicePlugin::MFDServicePlugin(
                                    MFD *owner,
                                    int identifier, 
                                    uint port, 
                                    const QString &a_name,
                                    bool l_use_thread_pool, 
                                    uint l_minimum_thread_pool_size
                                  )
                 :MFDBasePlugin(owner, identifier, a_name)
{
    if(l_minimum_thread_pool_size > 20)
    {
        warning("was asked to create a crazy number of minimum threads, reducing to 20");
        l_minimum_thread_pool_size = 20;
    }
    use_thread_pool = l_use_thread_pool;
    thread_pool_size = l_minimum_thread_pool_size;
    minimum_thread_pool_size = l_minimum_thread_pool_size;
    client_socket_identifier = 0;
    port_number = port;
    core_server_socket = NULL;
    client_sockets.setAutoDelete(true);
    if(pipe(u_shaped_pipe) < 0)
    {
        warning("could not create a u shaped pipe");
    }
    
    //
    //  Set default timeout values for select()'ing
    //
    
    time_wait_seconds = 60;
    time_wait_usecs = 0;
    
    //
    //  Create a pool of threads to handle incoming data/request from
    //  clients if the object inheriting this class wants a thread pool
    //
    
    if(use_thread_pool)
    {
        for (uint i = 0; i < thread_pool_size; i++) 
        {
            ServiceRequestThread *srt = new ServiceRequestThread(this);
            srt->start();
            thread_pool.push_back(srt);
        }
    }

    //
    //  Set our thread pool exhaustion timestamp
    //
    
    thread_exhaustion_timestamp.start();
}

void MFDServicePlugin::run()
{
    if(!initServerSocket())
    {
        warning(QString("could not init server socker on port %1")
                .arg(port_number));
    }

    while(keep_going)
    {

        //
        //  Update the status of our sockets.
        //
        
        updateSockets();
        waitForSomethingToHappen();
        checkInternalMessages();
        checkMetadataChanges();
    }

}

void MFDServicePlugin::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
    wakeUp();
}



bool MFDServicePlugin::initServerSocket()
{

    core_server_socket = new QSocketDevice();
    core_server_socket->setAddressReusable(true);
    if(!core_server_socket->bind(QHostAddress(), port_number))
    {
        return false;
    }
    if(!core_server_socket->listen(50))  // Qt manual says "50 is quite common"
    {
        return false;
    }
    core_server_socket->setBlocking(false);
    
    return true;
}

void MFDServicePlugin::updateSockets()
{

    //
    //  See if we have any new connections.
    //

    findNewClients();

    //
    //  drop any dead connections
    //
    
    dropDeadClients();

    //
    //  Get any incoming commands
    //
    
    readClients();


}

void MFDServicePlugin::findNewClients()
{
    bool new_connections = true;
    int new_socket_id;
    
    while(new_connections)
    {
        new_socket_id = core_server_socket->accept();
        if(new_socket_id > -1)
        {
            MFDServiceClientSocket *new_client = 
            new MFDServiceClientSocket(
                                    bumpClient(),
                                    new_socket_id,
                                    QSocketDevice::Stream
                                   );
                                   
            //
            //  Add this client to the list
            //
            
            client_sockets_mutex.lock();
                client_sockets.append(new_client);
            client_sockets_mutex.unlock();

            log(QString("has new client (total now %1)")
                .arg(client_sockets.count()), 3);
            
        }
        else
        {
            new_connections = false;
        }
    }
}

void MFDServicePlugin::dropDeadClients()
{

    client_sockets_mutex.lock();
        QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
        MFDServiceClientSocket *a_client;
        while ( (a_client = iterator.current()) != 0 )
        {
            ++iterator;
            
            if(a_client->socket() < 0)
            {
                //
                //  This occasionaly happens when the other end closes the
                //  connection ...
                //

                a_client->lockWriteMutex();
                a_client->unlockWriteMutex();
                client_sockets.remove(a_client);
                log(QString("lost a client (total now %1)")
                            .arg(client_sockets.count()), 3);
            }
            else
            {
                bool still_there = true;
                if(a_client->waitForMore(30, &still_there) < 1)
                {
                    if(!still_there)
                    {
                        //
                        //  No bytes to read, and we didn't even make it
                        //  through the 30 msec's .... client gone away. We
                        //  want to remove it from the list ... BUT, if the
                        //  writeLock is on, some other thread has not yet
                        //  realized this client has gone away ..
                        //
                        //  ... so, sit here and block (bah!) till that
                        //  ... other thread realizes what's going on
                        //
                    
                        a_client->lockWriteMutex();
                    
                        //
                        //  ah, all ours
                        //
                    
                        a_client->unlockWriteMutex();

                        client_sockets.remove(a_client);

                        log(QString("lost a client (total now %1)")
                            .arg(client_sockets.count()), 3);
                    }
                }
            }
        }
    client_sockets_mutex.unlock();
}

void MFDServicePlugin::readClients()
{
    if(!use_thread_pool)
    {
        warning("is calling readClients(), but it asked that there be no thread pool");
        return;
    }

    //
    //  Find anything with pending data and launch a thread to deal with the
    //  incoming request/command
    //

    client_sockets_mutex.lock();
        QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
        MFDServiceClientSocket *a_client;
        while ( (a_client = iterator.current()) != 0 )
        {
            ++iterator;
            if(a_client->bytesAvailable() > 0 && !a_client->isReading())
            {
                //
                //  Find a free processing thread and ask it to deal
                //  with this incoming data
                //
            
                ServiceRequestThread *srt = NULL;
                while (!srt)
                {
                    thread_pool_mutex.lock();
                        if (!thread_pool.empty())
                        {
                            srt = thread_pool.back();
                            thread_pool.pop_back();
                            if(thread_pool.empty())
                            {
                                thread_exhaustion_timestamp.restart();
                            }
                        }
                        else
                        {
                            thread_exhaustion_timestamp.restart();
                        }
                    thread_pool_mutex.unlock();

                    if (!srt)
                    {
                        if(thread_pool_size < 100) //  100 is totally arbitrary upper limit FIX?
                        {
                            //
                            //  Make the thread pool bigger by one, as we clearly need an additional thread
                            //
                        
                        
                            ServiceRequestThread *new_srt = new ServiceRequestThread(this);
                            new_srt->start();
                            thread_pool_mutex.lock();
                                thread_pool.push_back(new_srt);
                            thread_pool_mutex.unlock();
                            ++thread_pool_size;
                            log(QString("added another request thread on demand (total now %1)")
                                .arg(thread_pool_size), 1);
                        
                        }
                        else
                        {
                            warning(QString("waiting for free threads: thread pool size is already at %1")
                                    .arg(thread_pool_size));
                            usleep(50);
                        }
                    }
                }

                a_client->setReading(true);
                srt->handleIncoming(a_client);
            }
        }
    client_sockets_mutex.unlock();
}

void MFDServicePlugin::sendMessage(const QString &message, int socket_identifier)
{
    bool message_sent = false;
    QString newlined_message = message;
    newlined_message.append("\n");

    client_sockets_mutex.lock();
        QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
        MFDServiceClientSocket *a_client;
        while ( (a_client = iterator.current()) != 0 )
        {
            ++iterator;
            if(a_client->getIdentifier() == socket_identifier)
            {
                message_sent = true;
                a_client->lockWriteMutex();
                    int length = a_client->writeBlock(newlined_message.ascii(), newlined_message.length());
                a_client->unlockWriteMutex();
                if(length < 0)
                {
                    warning("client socket could not accept data");
                }
                else if(length != (int) newlined_message.length())
                {
                    warning("client socket did not consume correct amount of data");
                }
                break;
            }
        }
    client_sockets_mutex.unlock();

    if(!message_sent)
    {
        log("wanted to send a message, but the client socket in question went away", 7);
    }
}


void MFDServicePlugin::sendMessage(const QString &message)
{
    QString newlined_message = message;
    newlined_message.append("\n");

    client_sockets_mutex.lock();
        QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
        MFDServiceClientSocket *a_client;
        while ( (a_client = iterator.current()) != 0 )
        {
            ++iterator;
            a_client->lockWriteMutex();
            int length = a_client->writeBlock(newlined_message.ascii(), newlined_message.length());
            a_client->unlockWriteMutex();
            if(length < 0)
            {
                warning("client socket could not accept data");
            }
            else if(length != (int) newlined_message.length())
            {
                warning("client socket did not consume correct amount of data");
            }
        }
    client_sockets_mutex.unlock();
}

void MFDServicePlugin::sendInternalMessage(const QString &the_message)
{
    //
    //  This would typically be called by another object in another thread,
    //  and it gets put on a queue. The actual plugin will check for it
    //  in checkInternalMessages()
    //
    
    internal_messages_mutex.lock();
        internal_messages.push_back(the_message);
    internal_messages_mutex.unlock();
    wakeUp();
}

void MFDServicePlugin::checkInternalMessages()
{
    QString message = "";
    internal_messages_mutex.lock();
        if(!internal_messages.isEmpty() > 0)
        {
            message = internal_messages.first();
            internal_messages.pop_front();
        }
    internal_messages_mutex.unlock();
    if(message.length() > 0)
    {
        handleInternalMessage(message);
    }
}

void MFDServicePlugin::checkMetadataChanges()
{
    metadata_changed_mutex.lock();
        std::deque<MetadataEvent>::iterator iter = metadata_events.begin();
        for (; iter != metadata_events.end(); iter++)
        {
            handleMetadataChange((*iter).getCollectionId(), (*iter).getExternal());
        }
        metadata_events.clear();
    metadata_changed_mutex.unlock();
}

void MFDServicePlugin::checkServiceChanges()
{
    services_changed_mutex.lock();
        if(services_changed_flag)
        {
            services_changed_flag= false;
            handleServiceChange();
        }
    services_changed_mutex.unlock();    
}

void MFDServicePlugin::handleInternalMessage(QString)
{
    warning("has not re-implemented handleInternalMessage()");
}

void MFDServicePlugin::wakeUp()
{
    //
    //  Tell the main thread to wake up by sending some data to ourselves on
    //  our u_shaped_pipe. This may seem odd. It isn't.
    //

    u_shaped_pipe_mutex.lock();
        write(u_shaped_pipe[1], "wakeup\0", 7);
    u_shaped_pipe_mutex.unlock();
}

void MFDServicePlugin::checkThreadPool()
{
    //
    //  If we've gone 15 minutes without exhausting the pool, rip one out
    //
    //  Why 15 minutes? Why not? This is like setting a screen saver ... 
    //

    if( (thread_exhaustion_timestamp.elapsed() / 1000) > 15 * 60 &&
        thread_pool_size > minimum_thread_pool_size)
    {
        thread_exhaustion_timestamp.restart();
        thread_pool_mutex.lock();
        if(!thread_pool.empty())
        {
            ServiceRequestThread *srt = thread_pool.back();
            thread_pool.pop_back();
            --thread_pool_size;
            srt->killMe();
            srt->wait();
            delete srt;
            srt = NULL;
            log(QString("thread pool reduced by one due to lack of demand (size is now %1)")
                .arg(thread_pool_size), 1);
        }
        thread_pool_mutex.unlock();
    }
}

void MFDServicePlugin::waitForSomethingToHappen()
{
    //
    //  If we're using threads and it's been a "long time" since our thread
    //  pool was exhausted, drop the number of threads to conserve resources
    //  (reduse, reuse, recycle!!!)
    //

    if(use_thread_pool)
    {
        checkThreadPool();
    }

    int nfds = 0;
    fd_set readfds;

    FD_ZERO(&readfds);


    //
    //  Add the server socket to things we want to watch
    //

    if(port_number > 0)
    {
        if(core_server_socket)
        {
            int s_socket = core_server_socket->socket();
            if(s_socket > 0)
            {
                FD_SET(s_socket, &readfds);
                if(nfds <= s_socket)
                {
                    nfds = s_socket + 1;
                }
            }
            else
            {
                warning("core server socket invalid ... total mayhem");
            }
        }
        else
        {
            warning(QString("lost core server socket on port %1 "
                            "... impending doom")
                            .arg(port_number));
        }
    }

    //
    //  Next, add any client sockets that are already busy being read
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
    //  And now, any other file descriptors we were told to watch
    //
    
    other_file_descriptors_mutex.lock();
        QValueList<int>::iterator fd_it;
        for ( fd_it  = other_file_descriptors_to_watch.begin(); 
              fd_it != other_file_descriptors_to_watch.end(); 
              ++fd_it )
        {
            FD_SET((*fd_it), &readfds);
            if(nfds <= (*fd_it))
            {
                nfds = (*fd_it) + 1;
            }
        }
    other_file_descriptors_mutex.unlock();
    

    //
    //  Finally, add the control pipe
    //
            
    FD_SET(u_shaped_pipe[0], &readfds);
    if(nfds <= u_shaped_pipe[0])
    {
        nfds = u_shaped_pipe[0] + 1;
    }
    
    timeout_mutex.lock();
        timeout.tv_sec = time_wait_seconds;
        timeout.tv_usec = time_wait_usecs;
    timeout_mutex.unlock();

    //
    //  Sit in select() until data arrives
    //
    
    int result = select(nfds, &readfds, NULL, NULL, &timeout);
    if(result < 0)
    {
        warning("file descriptors watcher got an error on select()");
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
    
}

void MFDServicePlugin::setTimeout(int numb_seconds, int numb_useconds)
{
    timeout_mutex.lock();
        time_wait_seconds = numb_seconds;
        time_wait_usecs = numb_useconds;
    timeout_mutex.unlock();
}


int MFDServicePlugin::bumpClient()
{
    //
    //  Why do we have these identifiers? Because if a client sends a
    //  request, goes away before the response is ready, and then a totally
    //  different client connects, that new client may get the same socket
    //  that the client that disappeared left behind. This makes sure (?)
    //  that responses only go to the client that actually sent the request.
    //

    ++client_socket_identifier;
    return client_socket_identifier;
}

void MFDServicePlugin::processRequest(MFDServiceClientSocket *a_client)
{

    if(!a_client)
    {
        warning("tried to run a processRequest() on a client that does not exist");
        return;
    }

    //
    //  The thread pool stuff means that this gets called in its own thread.
    //
    //  We just pull out what's in there and fire off to doSomething()
    //

    char incoming[MAX_CLIENT_INCOMING];    // FIX
    int length = 0;
    
    a_client->lockReadMutex();
        length = a_client->readBlock(incoming, MAX_CLIENT_INCOMING - 1);
    a_client->unlockReadMutex();
    a_client->setReading(false);

    if(length > 0)
    {
        if(length >= MAX_CLIENT_INCOMING)
        {
            // oh crap
            warning("client socket is getting too much data");
            length = MAX_CLIENT_INCOMING;
        }
    
        incoming[length] = '\0';

        QString incoming_data = incoming;
        incoming_data = incoming_data.replace( QRegExp("\n"), "" );
        incoming_data = incoming_data.replace( QRegExp("\r"), "" );
        incoming_data.simplifyWhiteSpace();

        doSomething(QStringList::split(" ", incoming_data), a_client->getIdentifier());
    }
    
}

void MFDServicePlugin::markUnused(ServiceRequestThread *which_one)
{
    thread_pool_mutex.lock();
        thread_pool.push_back(which_one);
    thread_pool_mutex.unlock();
}

void MFDServicePlugin::doSomething(const QStringList&, int)
{
    warning("has not re-implemented doSomething()");
}

void MFDServicePlugin::handleMetadataChange(int, bool)
{
    //
    //  Default implementation is to do nothing
    //
}

void MFDServicePlugin::handleServiceChange()
{
    //
    //  Default implementation is to just warn
    //
    
    warning("services changed being handled by base class MFDServicePlugin::handleServiceChange()");

}

void MFDServicePlugin::addFileDescriptorToWatch(int fd)
{
    other_file_descriptors_mutex.lock();
        if( other_file_descriptors_to_watch.find(fd) != other_file_descriptors_to_watch.end())
        {
            warning("asked to add a file descriptor to watch that is already being watched");
        }
        else
        {
            other_file_descriptors_to_watch.append(fd);
        }
    other_file_descriptors_mutex.unlock();
}

void MFDServicePlugin::removeFileDescriptorToWatch(int fd)
{
    other_file_descriptors_mutex.lock();
        if( other_file_descriptors_to_watch.find(fd) == other_file_descriptors_to_watch.end())
        {
            warning("asked to remove a file descriptor to watch that is not being watched");
        }
        else
        {
            int numb_removed = other_file_descriptors_to_watch.remove(fd);
            if(numb_removed != 1)
            {
                warning("removed a file descriptor to watch, and more than one came off");
            }
        }
    other_file_descriptors_mutex.unlock();
}

MFDServicePlugin::~MFDServicePlugin()
{
    //
    //  Shut down all the processing threads
    //
    
    if(use_thread_pool)
    {
        while(thread_pool.size() < thread_pool_size)
        {
            sleep(1);
            warning("waiting for request threads to finish so it can exit");
        }

        for(uint i = 0; i < thread_pool_size; i++)
        {
            ServiceRequestThread *srt = thread_pool[i];
            if(srt)
            {
                srt->killMe();
                srt->wait();
                delete srt;
                srt = NULL;
            }
        }
    }
    
    //
    //  Make sure we have a lock on the client sockets, and then close them
    //

    client_sockets_mutex.lock();
        QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
        MFDServiceClientSocket *a_client;
        while ( (a_client = iterator.current()) != 0 )
        {
            ++iterator;
            a_client->lockWriteMutex();
                    
                //
                //  ah, all ours
                //
                    
                a_client->flush();
                a_client->close();
            a_client->unlockWriteMutex();

        }


        //
        //  They're all closed, get rid of them
        //    

        client_sockets.clear();

    client_sockets_mutex.unlock();

    //
    //  Wipe out the server socket
    //

    if(core_server_socket)
    {
        core_server_socket->flush();
        core_server_socket->close();
        delete core_server_socket;
        core_server_socket = NULL;
    }
    


}



