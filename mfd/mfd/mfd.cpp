/*
	mfd.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for the core mfd object

*/

#include "../config.h"

#include <signal.h>

#include <qapplication.h>
#include <qregexp.h>
#include <qtimer.h>

#include "mfd.h"
#include "../mfdlib/mfd_events.h"
#include "mdserver.h"

MFD::MFD(QSqlDatabase *ldb, int port, bool log_stdout, int logging_verbosity)
    :QObject()
{
    //
    //  Set some startup values
    //  
    
    connected_clients.clear();
    shutting_down = false;
    watchdog_flag = false;
    port_number = port;
    //metadata_container_identifier = 0;
    //metadata_video_generation = 0;
    //metadata_audio_generation = 4;    // 4? Why 4? ... arbitrary starting point, higher than 3 for obscure iTunes reasons
    //metadata_containers = new QPtrList<MetadataContainer>;
    //metadata_id = 1;
    //playlist_id = 1;

    //
    //  Assign the database if one exists
    //
    
    db = NULL;
    if(ldb)
    {
        db = ldb;
    }
    
    //
    //  Create the log (possibly to stdout)
    //
    
    mfd_log = new MFDLogger(log_stdout, logging_verbosity);
    mfd_log->addStartup();
    connect(this, SIGNAL(writeToLog(int, const QString &)), 
            mfd_log, SLOT(addEntry(int, const QString &)));
    log(QString("mfd is listening on port %1").arg(port), 1);
           
    
    //
    //  Create the server socket
    //
    
    server_socket = new MFDServerSocket(port);
    connect(server_socket, SIGNAL(newConnect(MFDClientSocket *)),
            this, SLOT(newConnection(MFDClientSocket *)));
    connect(server_socket, SIGNAL(endConnect(MFDClientSocket *)),
            this, SLOT(endConnection(MFDClientSocket *)));

    //
    //  Create the metadata server object (separate thread object that has
    //  it's own server port for serving metadata and owns all the
    //  containers)
    //
    
    metadata_server = new MetadataServer(this, 2344);
    metadata_server->start();

    //
    //  Create the plugin manager, which will
    //  automatically load the plugins
    //
    
    plugin_manager = new MFDPluginManager(this);
    connect(plugin_manager, SIGNAL(allPluginsLoaded(void)),
            this, SLOT(registerMFDService(void)));
    registerMFDService();
    
}


/*
void MFD::makeMetadataContainers()
{
    //
    //  This is only called at startup (constructor). It tries to build a
    //  list of available metadata collections based on either mythlib/gContext
    //  info or some guesses.
    //
    

#ifdef MYTHLIB_SUPPORT    
        //
        //  We have a gContext, so build it that way.
        //

        log("mfd will attempt to build initial metadata collections from myth database", 2);
        
        //
        //  If there is a musicmetadata table, build a DB-based collection
        //  that is:
        //
        //      content  = audio
        //      location = (this) host
        //
         
        MetadataContainer *new_collection = new MetadataMythDBContainer(
                                                                        this,
                                                                        bumpMetadataContainerIdentifier(),
                                                                        MCCT_audio,
                                                                        MCLT_host,
                                                                        db
                                                                       );
                                                                       
        metadata_containers->append(new_collection);
        



        MetadataContainer *another_new_collection = new MetadataMythDBContainer(
                                                                        this,
                                                                        bumpMetadataContainerIdentifier(),
                                                                        MCCT_audio,
                                                                        MCLT_host,
                                                                        db
                                                                       );

                                                                       
        metadata_containers->append(another_new_collection);

        
#else

        //
        //  We have to try and build collections by guessing about the filesystem
        //

        log("mfd will attempt to build initial metadata collections based on filesystem guesses", 2);
        
        //
        //  Make a set of directories to look in that might contain audio
        //  files (these are just guesses).
        //
        
        QStringList audio_locations;
        audio_locations.append("/mnt/store");
        audio_locations.append("/content");
#endif
    
}

*/

void MFD::customEvent(QCustomEvent *ce)
{

    //
    //  This method is fired when any of the
    //  sub-modules and/or their sub-threads
    //  post a custom event. It's a thread
    //  safe way to get data back up to the
    //  core mfd
    //
    
    if(ce->type() == 65432)
    {
        //
        //  It's a logging event
        //

        LoggingEvent *le = (LoggingEvent*)ce;
        QString log_string = le->getString();
        int log_verbosity = le->getVerbosity();
        emit writeToLog(log_verbosity, log_string);
    }
    else if(ce->type() == 65431)
    {
        //
        //  Socket Event (probably from a thread somewhere)
        //

        SocketEvent *se = (SocketEvent*)ce;

        int unique_identifier = se->getSocketIdentifier();
        
        //
        //  OK, we have a socket identifier. The problem is, the 
        //  socket it referenced may have gone away while the
        //  thread this came from was busy doing something.
        //
        //  We test against our known set and only send if we
        //  have a match. 
        //
        
        bool message_sent = false;
        if(unique_identifier == -1)
        {
            //
            //  This was a message sent from the mfd, not from a client
            //

            message_sent = true;
        }
        else
        {
            QPtrListIterator<MFDClientSocket> iterator( connected_clients );
            MFDClientSocket *a_socket;
            while ( (a_socket = iterator.current()) != 0 )
            {
                ++iterator;
                if(a_socket->getIdentifier() == unique_identifier)
                {
                    sendMessage(a_socket, se->getString());
                    message_sent = true;
                    break;
                }
            }
        }        
        if(!message_sent)
        {
            log(QString("wanted to send the following response but the socket/client that asked for it no longer exists: %1")
                .arg(se->getString()), 8);
        }
    }
    else if(ce->type() == 65430)
    {
        //
        //  Text to go to all connected clients
        //
        
        AllClientEvent *ace = (AllClientEvent*)ce;
        sendMessage(ace->getString());
    }
    else if(ce->type() == 65429)
    {
        //
        //  A plugin is sending a FatalEvent. Shut it down
        //  and kill it off.
        //  
        
        FatalEvent *fe = (FatalEvent*)ce;
        error(fe->getString());
        sendMessage("fatal");
        plugin_manager->stopPlugin(fe->getPluginIdentifier());
    }
    else if(ce->type() == 65428)
    {
        //
        //  A plugin wants to register a service
        //  
        
        ServiceEvent *se = (ServiceEvent*)ce;
        QStringList service_tokens = QStringList::split(" ", se->getString());
        if(!plugin_manager->parseTokens(service_tokens, -1))
        {
            warning(QString("mfd could not register this service at the request of a plugin: %1")
                    .arg(se->getString()));
        }
    }
    else if(ce->type() == 65427)
    {
        //
        //  Some metadata sweeper/monitor claims their data has changed. We
        //  iterate over the containers and tell the relevant one to update
        //  
        
        /*
        MetadataChangeEvent *mce = (MetadataChangeEvent*)ce;
        QPtrListIterator<MetadataContainer> iterator( *metadata_containers );
        MetadataContainer *a_container;
        while ( (a_container = iterator.current()) != 0 )
        {
            ++iterator;
            if(a_container->getIdentifier() == mce->getIdentifier())
            {
                if(a_container->tryToUpdate())
                {
                    if(a_container->isAudio())
                    {
                        bumpMetadataAudioGeneration();
                        log(QString("mfd metadata audio generation counter is now at %1")
                            .arg(metadata_audio_generation), 2);
                    }
                    else if (a_container->isVideo())
                    {
                        bumpMetadataAudioGeneration();
                        log(QString("mfd metadata video generation counter is now at %1")
                            .arg(metadata_video_generation), 2);
                    }
                    
                    metadata_server->startMetadataCheck();
                }
                break;
            }
         }
         */
    }
    else
    {
        warning("receiving custom events of a type I do not understand");
    }
}

void MFD::newConnection(MFDClientSocket *socket)
{
    //
    //  Add this client to the list
    //
    
    connected_clients.append(socket);

    log(QString("new socket connection from %1 (there are now %2 connected client(s))")
        .arg(socket->peerAddress().toString())
        .arg(connected_clients.count()), 2);


    //
    //  Whenever this client sends data,
    //  we should call the mfd's
    //  readSocket() method.
    //

    connect(socket, SIGNAL(readyRead()),
            this, SLOT(readSocket()));
            
}

void MFD::endConnection(MFDClientSocket *socket)
{
    //
    //  Remove this client from the list
    //

    if(!connected_clients.remove(socket))
    {
        warning("mfd.o: Asked to remove a client socket that doesn't exist");
    }
    else
    {
        log(QString("a socket connection has closed (there are now %1 connected client(s))")
            .arg(connected_clients.count()), 2);
    }
}

void MFD::readSocket()
{
    //
    //  Take in the data from the socket a line at a time.
    //  If there's something there after cleaning it, send
    //  to parseTokens()
    //

    MFDClientSocket *socket = (MFDClientSocket *)sender();
    if(socket->canReadLine())
    {
        QString incoming_data = socket->readLine();
        incoming_data = incoming_data.replace( QRegExp("\n"), "" );
        incoming_data = incoming_data.replace( QRegExp("\r"), "" );
        incoming_data.simplifyWhiteSpace();
        log(QString("incoming socket command from %1: %2")
            .arg(socket->peerAddress().toString())
            .arg(incoming_data),10);
        QStringList tokens = QStringList::split(" ", incoming_data);
        if(tokens.count() > 0)
        {
            parseTokens(tokens, socket);
        }
    }
}

void MFD::log(const QString &warning_text, int verbosity=1)
{
    //
    //  Why on earth do we create a logging event
    //  rather than just writing to the log? So
    //  that all logging happens in the right order
    //  (because every other object/thread/plugin is
    //  using logging events, and we'd like things to
    //  stay in sync).
    //
    
    LoggingEvent *le = new LoggingEvent(warning_text, verbosity);
    QApplication::postEvent(this, le);
}

void MFD::warning(const QString &warning_text)
{
    QString the_warning = warning_text;
    the_warning.prepend("WARNING: ");
    log(the_warning);
}

void MFD::error(const QString &error_text)
{
    QString the_error = error_text;
    the_error.prepend("ERROR: ");
    log(the_error);
}

void MFD::parseTokens(const QStringList &tokens, MFDClientSocket *socket)
{
    //
    //  parse commands coming in from the socket
    //  Check for the few commands the MFD understands
    //  on its own first.
    //

    if(tokens[0] == "halt" ||
       tokens[0] == "quit" ||
       tokens[0] == "shutdown")
    {
        if(! shutting_down)
        {
            shutting_down = true;
            plugin_manager->setShutdownFlag(true);
            log("connected client asked us to shutdown", 3);
            shutDown();
        }
    }
    else if(tokens[0] == "hello")
    {
        sendMessage(socket, "greetings");
    }
    else if(tokens[0] == "reload")
    {
        sendMessage("restart");
        plugin_manager->reloadPlugins();
    }
    else if(tokens[0] == "capabilities")
    {
        doListCapabilities(tokens, socket);
    }
    else if(plugin_manager->parseTokens(tokens, socket->getIdentifier()))
    {
        //
        //  Some plugin was willing to take the command line
        //
    }
    else
    {
        warning(QString("failed to parse this incoming command: %1").arg(tokens.join(" ")));
        sendMessage(socket, QString("huh %1").arg(tokens.join(" ")));
    }
}

void MFD::doListCapabilities(const QStringList& tokens, MFDClientSocket *socket)
{
    if(tokens.count() < 2)
    {
        warning(QString("failed to parse this incoming command: %1").arg(tokens.join(" ")));
        sendMessage(socket, QString("huh %1").arg(tokens.join(" ")));
        return;
    }
    if(tokens[1] != "list")
    {
        warning(QString("failed to parse this incoming command: %1").arg(tokens.join(" ")));
        sendMessage(socket, QString("huh %1").arg(tokens.join(" ")));
        return;
    }

    sendMessage(socket, "capability hello");
    sendMessage(socket, "capability reload");
    sendMessage(socket, "capability capabilities");
    sendMessage(socket, "capability halt");
    sendMessage(socket, "capability quit");
    sendMessage(socket, "capability shutdown");
    plugin_manager->doListCapabilities(socket->getIdentifier());
        
}

void MFD::shutDown()
{
    log("beginning shutdown", 1);

    //
    //  Clean things up and go away.
    //
    
    sendMessage("bye");
    
    //
    //  Close the socket(s)
    //

    MFDClientSocket *socket;
    while(connected_clients.count() > 0)
    {
        socket = connected_clients.getFirst();
        if(socket->state() == QSocket::Closing)
        {
            //
            //  We already asked this one to close
            //  but it ain't done yet.
            //
        }
        else
        {
            socket->close();
            if(socket->state() != QSocket::Closing)
            {
                endConnection(socket);
            }
        }
        qApp->processEvents();
    }
    
    //
    //  Ask the plugins to stop. Asking them to stop,
    //  however, does not mean they will immediately
    //  do so. So we wait .... 
    //
    //  But, we don't want a misbehaved plugin to
    //  prevent us from ever exiting, so we also set
    //  a timer.
    //
    
    QTimer *plugin_watchdog = new QTimer();
    connect(plugin_watchdog, SIGNAL(timeout()),
            this, SLOT(raiseWatchdogFlag()));
    plugin_watchdog->start(30000);   // 30 seconds
    plugin_manager->stopPlugins();
    while(plugin_manager->pluginsExist())
    {
        qApp->processEvents();
        if(watchdog_flag)
        {
            warning("the plugin manager failed to stop and unload all plugins within a reasonable amount of time");
            qApp->processEvents();
            break;
        }
    }

    //
    //  Turn off the metadata
    //
    
    metadata_server->stop();
    metadata_server->wait();
    
    
    //
    //  Log the shutdown
    //

    mfd_log->addShutdown();
    exit(0);
}



void MFD::sendMessage(MFDClientSocket *where, const QString &what)
{
    //
    //  Send command/message to a specific 
    //  client socket
    //

    QString message = what;
    log(QString("sending the following message to %1: %2")
        .arg(where->peerAddress().toString())
        .arg(message), 10);
       
    message.append("\n");

    where->writeBlock(message.latin1(), message.length());
}

void MFD::sendMessage(const QString &the_message)
{
    //
    //  send command/message to every
    //  connected client
    //

    QPtrListIterator<MFDClientSocket> iterator( connected_clients );
    MFDClientSocket *socket;
    while ( (socket = iterator.current()) != 0 )
    {
        ++iterator;
        sendMessage(socket, the_message);
    }
}

void MFD::raiseWatchdogFlag()
{
    watchdog_flag = true;
}

void MFD::registerMFDService()
{
    //
    //  Assuming all the plugins have been (re-) loaded, we need to tell
    //  whatever plugin is handling services that we exist as an mfd
    //  service.
    //
    
    char my_hostname[2049];
    if(gethostname(my_hostname, 2048) < 0)
    {
        warning("mfd could not call gethostname()");
        return;
    }

    QString local_hostname = my_hostname;
    QString self_registration_string = QString("services add mfdp %1 mfd on %2")
                                       .arg(port_number)
                                       .arg(local_hostname);

    QStringList self_registration_tokens = QStringList::split(" ", self_registration_string);

    if(!plugin_manager->parseTokens(self_registration_tokens, -1))
    {
        warning("mfd could not register itself (perhaps no plugin handling services tokens?)");
    }
    
}

/*
int MFD::bumpMetadataContainerIdentifier()
{
    //
    //  Every metadatata collection (items and playlists) gets a unique id.
    //
    
    ++metadata_container_identifier;
    return metadata_container_identifier;
}

int MFD::bumpMetadataAudioGeneration()
{
    //
    //  Any time anything changes in any audio metadata collection, we need
    //  to keep track of the fact that something has changed by augmenting
    //  the value of metadata_audio_generation
    //
    
    ++metadata_audio_generation;
    return metadata_audio_generation;
}

int MFD::bumpMetadataVideoGeneration()
{
    ++metadata_video_generation;
    return metadata_video_generation;
}

int MFD::bumpMetadataId()
{
    //
    //  For ease of use with the other software (notably the somewhat brain
    //  dead iTunes), we want every metadata obect to get a unique id
    //
    
    int return_value;
    bump_metadata_id_mutex.lock();
    ++metadata_id;
    return_value = metadata_id;
    bump_metadata_id_mutex.unlock();
    return return_value;
}

int MFD::bumpPlaylistId()
{
    //
    //  For ease of use with the other software (notably the somewhat brain
    //  dead iTunes), we want every playlist obect to get a unique id
    //
    
    int return_value;
    bump_playlist_id_mutex.lock();
    ++playlist_id;
    return_value = playlist_id;
    bump_playlist_id_mutex.unlock();
    return return_value;
}


uint MFD::getAllAudioMetadataCount()
{
    //
    //  Iterate over all Audio collections and count the items
    //

    uint return_value;

    lockMetadata();

        return_value = 0;
        MetadataContainer * a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            if(a_container->isAudio())
            {
                return_value += a_container->getMetadataCount();
            }
        }
    
    unlockMetadata();
    return return_value;
}

uint MFD::getAllAudioPlaylistCount()
{
    //
    //  Iterate over all Audio collections and count the playlists
    //

    uint return_value;

    lockPlaylists();

        return_value = 0;
        MetadataContainer * a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            if(a_container->isAudio())
            {
                return_value += a_container->getPlaylistCount();
            }
        }
    
    unlockPlaylists();
    return return_value;

}

void MFD::lockMetadata()
{
    metadata_mutex.lock();
}

void MFD::unlockMetadata()
{
    metadata_mutex.unlock();
}

void MFD::lockPlaylists()
{
    playlist_mutex.lock();
}

void MFD::unlockPlaylists()
{
    playlist_mutex.unlock();
}

Metadata* MFD::getMetadata(int id)
{
    //
    //  Iterate over all Audio collections and return the requested metadata
    //

    Metadata* return_value = NULL;

    lockMetadata();

        MetadataContainer * a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            QIntDict<Metadata> *the_metadata = a_container->getMetadata();
            if(the_metadata->find(id))
            {
                return_value = the_metadata->find(id);
                a_container = metadata_containers->last();
            }
        }
    
    unlockMetadata();
    return return_value;
}

Playlist* MFD::getPlaylist(int id)
{
    //
    //  Iterate over all Audio collections and return the requested playlist
    //

    Playlist* return_value = NULL;

    lockPlaylists();

        MetadataContainer * a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            QIntDict<Playlist> *the_playlists = a_container->getPlaylists();
            if(the_playlists->find(id))
            {
                return_value = the_playlists->find(id);
                a_container = metadata_containers->last();
            }
        }
    
    unlockPlaylists();
    return return_value;
    
}

*/


MFD::~MFD()
{

    if(metadata_server)
    {
        delete metadata_server;
        metadata_server = NULL;
    }    

    if(plugin_manager)
    {
        delete plugin_manager;
        plugin_manager = NULL;
    }
    
    if(server_socket)
    {
        delete server_socket;
        server_socket = NULL;
    }
    
    if(mfd_log)
    {
        delete mfd_log;
        mfd_log = NULL;
    }
    
}


