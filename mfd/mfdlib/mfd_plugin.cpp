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


MFDBasePlugin::MFDBasePlugin(MFD *owner, int identifier)
{
    parent = owner;
    unique_identifier = identifier;
    keep_going = true;
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

MFDServicePlugin::MFDServicePlugin(MFD *owner, int identifier, int port)
                 :MFDBasePlugin(owner, identifier)
{
    client_socket_identifier = 0;
    port_number = port;
    core_server_socket = NULL;
    client_sockets.setAutoDelete(true);
    if(pipe(u_shaped_pipe) < 0)
    {
        warning("mfd service could not create a u shaped pipe");
    }
    
    //
    //  Add the read side of the pipe to the set of internal descriptors we
    //  watch. We can then wake this thread up just by writing to the write
    //  side of the pipe (see wakeUp())
    //

    internal_file_descriptors.append(u_shaped_pipe[0]);

    //
    //  Set default timeout values for select()'ing
    //
    
    time_wait_seconds = 5;
    time_wait_usecs = 0;
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
    
    //
    //  Add the fd for this socket to the list of internal descriptors we
    //  watch for changes on
    //

    internal_file_descriptors.append(core_server_socket->socket());
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
            client_sockets.append(new_client);
            internal_file_descriptors.append(new_client->socket());
        }
        else
        {
            new_connections = false;
        }
    }
}

void MFDServicePlugin::dropDeadClients()
{
    QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
    MFDServiceClientSocket *a_client;
    while ( (a_client = iterator.current()) != 0 )
    {
        ++iterator;
        bool still_there;
        if(a_client->waitForMore(100, &still_there) < 1)
        {
            if(!still_there)
            {
                //
                //  No bytes to read, and we didn't even make it through
                //  the 100 msec's .... client gone away.
                //

                if(!internal_file_descriptors.remove(a_client->socket()))
                {
                    warning("service plugin saw a client go away that was not on our list of internal descriptors");
                }

                client_sockets.remove(a_client);
            }
        }
    }
}

void MFDServicePlugin::addToDo(QString new_stuff_todo, int client_id)
{
    things_to_do_mutex.lock();
        SocketBuffer *new_request = new SocketBuffer(QStringList::split(" ", new_stuff_todo), client_id);
        things_to_do.append(new_request);
    things_to_do_mutex.unlock();
    wakeUp();
}

void MFDServicePlugin::readClients()
{
    char incoming[2049];

    QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
    MFDServiceClientSocket *a_client;
    while ( (a_client = iterator.current()) != 0 )
    {
        ++iterator;
        int length = a_client->readBlock(incoming, 2048);
        if(length > 0)
        {
            //
            //  Hopefully everything comes in line oriented. If not, things
            //  might get a wee bit screwy (?)
            //            
            if(length >= 2048)
            {
                // oh crap
                warning("service plugin client socket is getting too much data");
                length = 2048;
            }
            incoming[length] = '\0';
            QString incoming_data = incoming;
            incoming_data = incoming_data.replace( QRegExp("\n"), "" );
            incoming_data = incoming_data.replace( QRegExp("\r"), "" );
            incoming_data.simplifyWhiteSpace();
            
            //
            //  Put this incoming stuff in our buffer
            //   

            addToDo(incoming_data, a_client->getIdentifier());
        }
    }
}

void MFDServicePlugin::sendMessage(const QString &message, int socket_identifier)
{
    bool message_sent = false;
    QString newlined_message = message;
    newlined_message.append("\n");
    QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
    MFDServiceClientSocket *a_client;
    while ( (a_client = iterator.current()) != 0 )
    {
        ++iterator;
        if(a_client->getIdentifier() == socket_identifier)
        {
            message_sent = true;
            int length = a_client->writeBlock(newlined_message.ascii(), newlined_message.length());
            if(length < 0)
            {
                warning("service plugin client socket could not accept data");
            }
            else if(length != (int) newlined_message.length())
            {
                warning("service plugin client socket did not consume correct amount of data");
            }
            break;
        }
    }
    if(!message_sent)
    {
        log("wanted to send a message, but the service client socket in question went away", 8);
    }
}


void MFDServicePlugin::sendMessage(const QString &message)
{
    QString newlined_message = message;
    newlined_message.append("\n");
    QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
    MFDServiceClientSocket *a_client;
    while ( (a_client = iterator.current()) != 0 )
    {
        ++iterator;
        int length = a_client->writeBlock(newlined_message.ascii(), newlined_message.length());
        if(length < 0)
        {
            warning("service plugin client socket could not accept data");
        }
        else if(length != (int) newlined_message.length())
        {
            warning("service plugin client socket did not consume correct amount of data");
        }
    }
}

void MFDServicePlugin::wakeUp()
{
    //
    //  Tell the main thread to wake up by sending some data to ourselves on
    //  our u_shaped_pipe
    //

    u_shaped_pipe_mutex.lock();
        write(u_shaped_pipe[1], "wakeup\0", 7);
    u_shaped_pipe_mutex.unlock();
}

void MFDServicePlugin::addFileDescriptor(int fd)
{
    file_descriptors_mutex.lock();
        extra_file_descriptors.append(fd);
    file_descriptors_mutex.unlock();    
}

void MFDServicePlugin::removeFileDescriptor(int fd)
{
    file_descriptors_mutex.lock();
        if(!extra_file_descriptors.remove(fd))
        {
            warning("service plugin was asked to remove a descriptor that is not on our extra list");
        }
    file_descriptors_mutex.unlock();    
}

void MFDServicePlugin::clearFileDescriptors()
{
    file_descriptors_mutex.lock();
        extra_file_descriptors.clear();
    file_descriptors_mutex.unlock();    
}

void MFDServicePlugin::waitForSomethingToHappen()
{
    int nfds = 0;
    fd_set readfds;

    FD_ZERO(&readfds);
            
    file_descriptors_mutex.lock();


        QValueList<int>::iterator int_it;
        for (
                int_it  = internal_file_descriptors.begin(); 
                int_it != internal_file_descriptors.end(); 
                ++int_it
            ) 
        {
            FD_SET((*int_it), &readfds);
            if(nfds <= (*int_it))
            {
                nfds = (*int_it) + 1;
            }
        }
        for (
                int_it  = extra_file_descriptors.begin(); 
                int_it != extra_file_descriptors.end(); 
                ++int_it
            ) 
        {
            FD_SET((*int_it), &readfds);
            if(nfds <= (*int_it))
            {
                nfds = (*int_it) + 1;
            }
        }

    file_descriptors_mutex.unlock();    
    
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
        warning("service plugin file descriptors watcher got an error on select()");
    }
    
    //
    //  In case data came in on out u_shaped_pipe, clean it out
    //

    if(FD_ISSET(u_shaped_pipe[0], &readfds))
    {
        char read_back[10];
        read(u_shaped_pipe[0], read_back, 9);
    }
    
}

void MFDServicePlugin::setTimeout(int numb_seconds, int numb_useconds)
{
    timeout_mutex.lock();
        time_wait_seconds = numb_seconds;
        time_wait_usecs = numb_useconds;
    timeout_mutex.unlock();
}




MFDServicePlugin::~MFDServicePlugin()
{
    if(core_server_socket)
    {
        delete core_server_socket;
        core_server_socket = NULL;
    }
    
    client_sockets.clear();
    internal_file_descriptors.clear();
    extra_file_descriptors.clear();
}

/*
---------------------------------------------------------------------
*/

MFDServiceClientSocket::MFDServiceClientSocket(int identifier, int socket_id, Type type)
                    :QSocketDevice(socket_id, type)
{
    unique_identifier = identifier;
    setBlocking(false);
}                     


