    /*
	mfd_plugin.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for plugin skeleton

*/

#include <iostream>
using namespace std;

#include <qobject.h>
#include <qregexp.h>

#include "mfd_plugin.h"
#include "mfd_events.h"
#include "httpresponse.h"


MFDBasePlugin::MFDBasePlugin(MFD *owner, int identifier)
{
    parent = owner;
    unique_identifier = identifier;
    keep_going = true;
    name = "unknown";
}


void MFDBasePlugin::log(const QString &log_message, int verbosity)
{
    log_mutex.lock();
        if(parent)
        {
            LoggingEvent *le = new LoggingEvent(log_message, verbosity);
            QApplication::postEvent(parent, le);    
        }
    log_mutex.unlock();
}

void MFDBasePlugin::warning(const QString &warning_message)
{
    warning_mutex.lock();
        QString warn_string = warning_message;
        warn_string.prepend("WARNING: ");
        log(warn_string, 1);
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

void MFDBasePlugin::setName(const QString &a_name)
{
    name = a_name;
}

MFDBasePlugin::~MFDBasePlugin()
{
}

/*
---------------------------------------------------------------------
*/

MFDCapabilityPlugin::MFDCapabilityPlugin(MFD* owner, int identifier)
                    :MFDBasePlugin(owner, identifier)
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
    cerr << "Base mfdplugin's class doSomething() method was called. " << endl;
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
            warning(QString("an mfdplugin with unique identifier of %1 has more than %2 pending commands")
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

MFDServicePlugin::MFDServicePlugin(MFD *owner, int identifier, int port, bool l_use_thread_pool, uint l_thread_pool_size)
                 :MFDBasePlugin(owner, identifier)
{
    use_thread_pool = l_use_thread_pool;
    thread_pool_size = l_thread_pool_size;
    client_socket_identifier = 0;
    port_number = port;
    core_server_socket = NULL;
    client_sockets.setAutoDelete(true);
    if(pipe(u_shaped_pipe) < 0)
    {
        warning("mfd service plugin could not create a u shaped pipe");
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


    
}

void MFDServicePlugin::run()
{
    initServerSocket();

    while(keep_going)
    {

        //
        //  Update the status of our sockets.
        //
        
        updateSockets();
        waitForSomethingToHappen();
    }
}



bool MFDServicePlugin::initServerSocket()
{
    core_server_socket = new QSocketDevice();
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

            log(QString("%1 plugin has new client (total now %2)")
                .arg(name)
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
            bool still_there;
            if(a_client->waitForMore(30, &still_there) < 1)
            {
                if(!still_there)
                {
                    //
                    //  No bytes to read, and we didn't even make it through
                    //  the 30 msec's .... client gone away. We want to
                    //  remove it from the list ... BUT, if the writeLock is
                    //  on, some other thread has not yet realized this
                    //  client has gone away ..
                    //
                    //  ... so, sit here and block (bah!) till that other
                    //  ... thread realizes what's going on
                    
                    a_client->lockWriteMutex();
                    
                    //
                    //  ah, all ours
                    //
                    
                    a_client->unlockWriteMutex();

                    client_sockets.remove(a_client);

                    log(QString("%1 plugin lost a client (total now %2)")
                        .arg(name)
                        .arg(client_sockets.count()), 3);
                }
            }
        }
    client_sockets_mutex.unlock();
}

void MFDServicePlugin::readClients()
{
    if(!use_thread_pool)
    {
        warning(QString("%1 plugin is calling readClients(), but it asked that there be no thread pool")
                .arg(name));
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
                        }
                    thread_pool_mutex.unlock();

                    if (!srt)
                    {
                        warning(QString("%1 plugin is waiting for free threads ... increase its pool size?")
                                .arg(name));
                        usleep(50);
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
                    warning(QString("%1 plugin client socket could not accept data")
                            .arg(name));
                }
                else if(length != (int) newlined_message.length())
                {
                    warning(QString("%1 plugin client socket did not consume correct amount of data")
                            .arg(name));
                }
                break;
            }
        }
    client_sockets_mutex.unlock();

    if(!message_sent)
    {
        log(QString("%1 plugin wanted to send a message, but the client socket in question went away")
            .arg(name), 8);
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
                warning(QString("%1 plugin client socket could not accept data")
                        .arg(name));
            }
            else if(length != (int) newlined_message.length())
            {
                warning(QString("%1 plugin client socket did not consume correct amount of data")
                        .arg(name));
            }
        }
    client_sockets_mutex.unlock();
}

void MFDServicePlugin::sendCoreMFDMessage(const QString &message, int socket_identifier)
{
    MFDBasePlugin::sendMessage(message, socket_identifier);
}

void MFDServicePlugin::sendCoreMFDMessage(const QString &message)
{
    MFDBasePlugin::sendMessage(message);
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

void MFDServicePlugin::waitForSomethingToHappen()
{
    int nfds = 0;
    fd_set readfds;

    FD_ZERO(&readfds);


    //
    //  Add the server socket to things we want to watch
    //

    FD_SET(core_server_socket->socket(), &readfds);
    if(nfds <= core_server_socket->socket())
    {
        nfds = core_server_socket->socket() + 1;
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
            if(!a_client->isReading())
            {
                FD_SET(a_client->socket(), &readfds);
                if(nfds <= a_client->socket())
                {
                    nfds = a_client->socket() + 1;
                }
            }
        }
    client_sockets_mutex.unlock();

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
        warning(QString("%1 plugin file descriptors watcher got an error on select()")
                .arg(name));
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
        warning(QString("%1 plugin tried to run a processRequest() on a client that does not exist")
                .arg(name));
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
            warning(QString("%1 plugin client socket is getting too much data")
                    .arg(name));
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
    warning(QString("%1 plugin has not re-implemented doSomething()")
            .arg(name));
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
            warning(QString("%1 plugin is waiting for request threads to finish so it can exit")
                    .arg(name));
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
    


    if(core_server_socket)
    {
        delete core_server_socket;
        core_server_socket = NULL;
    }
    
    client_sockets.clear();

    
}



/*
---------------------------------------------------------------------
*/

//
//  Yes, it is an entire friggin' http server
//

MFDHttpPlugin::MFDHttpPlugin(MFD *owner, int identifier, int port)
                 :MFDServicePlugin(owner, identifier, port)
{
    initServerSocket();
}


MFDHttpPlugin::~MFDHttpPlugin()
{
    if(core_server_socket)
    {
        delete core_server_socket;
        core_server_socket = NULL;
    }
    
    client_sockets.clear();
}

void MFDHttpPlugin::run()
{
    while(keep_going)
    {

        //
        //  Update the status of our sockets.
        //
        
        updateSockets();
        waitForSomethingToHappen();
    }
}

void MFDHttpPlugin::processRequest(MFDServiceClientSocket *a_client)
{

    if(!a_client)
    {
        warning(QString("%1 httpd plugin tried to run a processRequest() on a client that does not exist")
                .arg(name));
        return;
    }

    char incoming[MAX_CLIENT_INCOMING];    // FIX
    int length = 0;
    int client_id = a_client->getIdentifier();

    //
    //  Copy out the whole block out so we can release the lock straight away.
    //


    a_client->lockReadMutex();
        length = a_client->readBlock(incoming, MAX_CLIENT_INCOMING - 1);
    a_client->unlockReadMutex();
    a_client->setReading(false);
    
    
    if(length > 0)
    {
        if(length >= MAX_CLIENT_INCOMING)
        {
            // oh crap
            warning(QString("%1 plugin client http socket is getting too much data")
                    .arg(name));
            length = MAX_CLIENT_INCOMING;
        }

        HttpRequest *new_request = new HttpRequest(incoming, length);
        if(new_request->allIsWell())
        {
            //
            //  Pass it to handleIncoming() which should be re-implemented
            //  in any actual plugin
            //
            
            handleIncoming(new_request);
            if(new_request->sendResponse())
            {
                sendResponse(client_id, new_request->getResponse());
            }
        }
        delete new_request;
    }
}


void MFDHttpPlugin::handleIncoming(HttpRequest *)
{
    warning(QString("%1 plugin called base class handleRequest()")
            .arg(name));
}

void MFDHttpPlugin::sendResponse(int client_id, HttpResponse *http_response)
{
    MFDServiceClientSocket *which_client = NULL;

    client_sockets_mutex.lock();
        QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
        MFDServiceClientSocket *a_client;
        while ( (a_client = iterator.current()) != 0 )
        {
            ++iterator;
            if(a_client->getIdentifier() == client_id)
            {
                which_client = a_client;
                break;
            }
        }
    client_sockets_mutex.unlock();

    if(!which_client)
    {
        log(QString("%1 plugin wanted to send an http response, but the client socket in question went away")
            .arg(name), 8);
    }
    a_client->lockWriteMutex();
        
        //
        //  Assemble and send the message
        //

        http_response->send(a_client);

    a_client->unlockWriteMutex();
}
