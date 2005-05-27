/*
	mfd.cpp

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
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
#include "servicelister.h"
#include "signalthread.h"
#include "settings.h"

extern SignalThread *signal_thread;

MFD::MFD(uint port, bool log_stdout, int logging_verbosity)
    :QObject()
{
    //
    //  Set some startup values
    //  
    
    connected_clients.clear();
    shutting_down = false;
    watchdog_flag = false;
    port_number = port;

    //
    //  Create a thread just to watch for INT/TERM signals and deal with them
    //
    
    signal_thread = new SignalThread(this);
    signal_thread->start();

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
    
    metadata_server = new MetadataServer(this, mfdContext->getNumSetting("mfd_metadata_port"));
    metadata_server->start();

    //
    //  Create the ServiceLister object (little thing that just keeps track
    //  of what services (http, daap, etc.) are available.
    //
    
    service_lister = new ServiceLister(this);

    //
    //  Create the plugin manager, which will
    //  automatically load the plugins
    //
    
    plugin_manager = new MFDPluginManager(this);

    //
    //  Announce the existence of this myth frontend daemon service
    //

    registerMFDService();
    
}


void MFD::customEvent(QCustomEvent *ce)
{

    //
    //  This method is fired when any of the
    //  sub-modules and/or their sub-threads
    //  post a custom event. It's a thread
    //  safe way to get data back up to the
    //  core mfd
    //

    // FIXME: Hardcoded constants.
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
            log(QString("wanted to send the following response but the socket"
                        "/client that asked for it no longer exists: %1")
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
        //  Let the service_lister object deal with these
        //

        ServiceEvent *se = (ServiceEvent*)ce;
        service_lister->handleServiceEvent(se);

        //
        //  Tell the plugins the event occured
        //  
        
        plugin_manager->tellPluginsServicesChanged();
        
    }
    else if(ce->type() == 65427)
    {
        //
        //  Metadata change event occured, tell the plugins that care.
        //        
        
        MetadataChangeEvent *mce = (MetadataChangeEvent*)ce;
        plugin_manager->tellPluginsMetadataChanged(
                                                    mce->getIdentifier(), 
                                                    mce->getExternalFlag()
                                                  );
                
    }
    else if(ce->type() == 65426)
    {
        //
        //  Time to shutdown!
        //        
        
        shutting_down = true;
        plugin_manager->setShutdownFlag(true);
        log("got a Shutdown Event, will call shutDown()", 1);
        shutDown();
                
    }
    else
    {
        warning(QString("receiving custom events of a type I do not "
                        "understand, the type was %1.").arg(ce->type()));
    }
}

void MFD::newConnection(MFDClientSocket *socket)
{
    //
    //  Add this client to the list
    //
    
    connected_clients.append(socket);

    log(QString("new socket connection from %1 (there are now %2 connected "
                "client(s))").arg(socket->peerAddress().toString())
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
        log(QString("a socket connection has closed (there are now %1 "
                    "connected client(s))").arg(connected_clients.count()), 2);
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
    else if(tokens[0] == "services")
    {
        doServiceResponse(tokens, socket);
    }
    else if(plugin_manager->parseTokens(tokens, socket->getIdentifier()))
    {
        //
        //  Some plugin was willing to take the command line
        //
    }
    else
    {
        warning(QString("failed to parse this incoming command: %1")
                        .arg(tokens.join(" ")));
        sendMessage(socket, QString("huh %1").arg(tokens.join(" ")));
    }
}

void MFD::doListCapabilities(const QStringList& tokens,
                             MFDClientSocket *socket)
{
    if(tokens.count() < 2)
    {
        warning(QString("failed to parse this incoming command: %1")
                        .arg(tokens.join(" ")));
        sendMessage(socket, QString("huh %1").arg(tokens.join(" ")));
        return;
    }
    if(tokens[1] != "list")
    {
        warning(QString("failed to parse this incoming command: %1")
                        .arg(tokens.join(" ")));
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

void MFD::doServiceResponse(const QStringList& tokens,
                             MFDClientSocket *socket)
{
    if(tokens.count() < 2)
    {
        warning(QString("failed to parse this incoming command: %1")
                        .arg(tokens.join(" ")));
        sendMessage(socket, QString("huh %1").arg(tokens.join(" ")));
        return;
    }
    if(tokens[1] == "list")
    {
        doServiceList(socket);
        return;
    }

    sendMessage(socket, QString("huh %1").arg(tokens.join(" ")));
}

void MFD::doServiceList(MFDClientSocket *socket)
{
    typedef     QValueList<Service> ServiceList;

    service_lister->lockDiscoveredList();
        ServiceList *discovered_list = service_lister->getDiscoveredList();
        ServiceList::iterator it;
        for ( it  = discovered_list->begin();
              it != discovered_list->end();
              ++it)
        {
            sendMessage(socket, (*it).getFormalDescription(true));
        }
        
    service_lister->unlockDiscoveredList();
}



void MFD::shutDown()
{
    log("beginning shutdown", 1);
    plugin_manager->setDeadPluginTimer(500);

    //
    //  Clean things up and go away.
    //
    
    sendMessage("bye");
    
    //
    //  Get rid of the signal thread
    //
    
    if(signal_thread)
    {
        signal_thread->stop();
        signal_thread->wait();
        delete signal_thread;
        signal_thread = NULL;
    }

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
            warning("the plugin manager failed to stop and unload all plugins "
                    "within a reasonable amount of time");
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
    log(QString("sending the following message to %1(%2): %3")
        .arg(where->peerAddress().toString())
        .arg(where->getIdentifier())
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

    Service *mfdp_service = new Service(
                                            QString("mfd on %1").arg(local_hostname),
                                            QString("mfdp"),
                                            local_hostname,
                                            SLT_HOST,
                                            port_number
                                        );
 
    ServiceEvent *se = new ServiceEvent( true, true, *mfdp_service);
   
    delete mfdp_service;


    QApplication::postEvent(this, se);

}

MFD::~MFD()
{

    if(metadata_server)
    {
        delete metadata_server;
        metadata_server = NULL;
    }    

    if(service_lister)
    {
        delete service_lister;
        service_lister = NULL;
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


