/*
	pluginmanager.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for the mfd plugin manager and wrapper

*/

#include <qapplication.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qregexp.h>

#include "pluginmanager.h"
#include "../mfdlib/mfd_events.h"
#include "mfd.h"



MFDPluginWrapper::MFDPluginWrapper(MFD *owner, const QString &file_name, int identifier)
          :QLibrary(file_name)
{
    
    acceptable_tokens.clear();
    parent = owner;
    unique_identifier = identifier;
}

bool MFDPluginWrapper::init()
{
    //
    //  load the plugin
    //
    
    this->load();

    //
    //  try to resolve (not execute!) the 4 necessary and 2 optional
    //  interface mechanisms
    //
    
    if(!(init_function = (Plugin_Init_Function) this->resolve("mfdplugin_init")) ||
       !(run_function = (Plugin_Run_Function) this->resolve("mfdplugin_run")) ||
       !(stop_function = (Plugin_Stop_Function) this->resolve("mfdplugin_stop")) ||
       !(can_unload_function = (Plugin_Can_Unload_Function) this->resolve("mfdplugin_can_unload")) )
    {
        warning(QString("plugin wrapper could not find required hooks into a plugin called %1")
                .arg(shortName()));
        return false;
    }

    if(!(parse_tokens_function = (Plugin_Parse_Tokens_Function) this->resolve("mfdplugin_parse_tokens")))
    {
        log(QString("plugin wrapper could interface with %1 but it has no parseTokens interface (has own service interface?)")
            .arg(shortName()),8);
    }

    if(!(capabilities_function = (Plugin_Capabilities_Function) this->resolve("mfdplugin_capabilities")))
    {
        log(QString("plugin wrapper could interface with %1 but it has no capabilites interface (has own service interface?)")
            .arg(shortName()),8);
    }

    if(!(metadata_change_function = (Plugin_Metadata_Change_Function) this->resolve("mfdplugin_metadata_change")))
    {
        log(QString("plugin wrapper did not find a metadata change interface in plugin %1")
            .arg(shortName()),6);
    }

    if(!(services_change_function = (Plugin_Services_Change_Function) this->resolve("mfdplugin_services_change")))
    {
        log(QString("plugin wrapper did not find a services change interface in plugin %1")
            .arg(shortName()),6);
    }

    //
    //  Now try to init
    //

    if(init_function)
    {
        //
        //  The init function resolved.
        //

        if(init_function(parent, unique_identifier))
        {
            //
            //  The init function seemed to work
            //
            return true;            
        }
    }
    return false;
}

void MFDPluginWrapper::getCapabilities()
{
    if(capabilities_function)
    {
        acceptable_tokens = capabilities_function();
    }
    else
    {
        acceptable_tokens.clear();
    }
    
    if(acceptable_tokens.count() < 1)
    {
        log(QString("plugin wrapper found that the plugin %1 has no capabilities (hopefully it has a service interface)")
        .arg(shortName()), 8);
    }
    else
    {
        log(QString("plugin wrapper found that plugin %1 has the following capabilities: %2")
            .arg(shortName())
            .arg(acceptable_tokens.join(", ")), 8);
    }
    
}

bool MFDPluginWrapper::run()
{
    if(run_function)
    {
        return run_function();
    }
    return false;
}

void MFDPluginWrapper::parseTokens(const QStringList &tokens, int socket_identifier)
{
    if(parse_tokens_function)
    {
        parse_tokens_function(tokens, socket_identifier);
        return;
    }
    warning(QString("plugin wrapper could not pass tokens to the following plugin: %1")
            .arg(shortName()));
}

void MFDPluginWrapper::stop()
{
    //
    //  A call that may take a while ... this asks the
    //  plugins to stop ('cause they're about to get
    //  killed off)
    //

    if(stop_function)
    {
        stop_function();
    }
    else
    {
        warning(QString("plugin wrapper could not get a plugin called %1 (unique identifier %2) to respond to stop()")
                .arg(shortName())
                .arg(unique_identifier));
    }
}

bool MFDPluginWrapper::acceptsToken(const QString &a_token)
{
    if(acceptable_tokens.contains(a_token))
    {
        return true;
    }
    return false;
}

const QString& MFDPluginWrapper::shortName()
{
    short_name = this->library();
    short_name = short_name.section('/', -1);
    short_name = short_name.remove("libmfdplugin-");
    short_name = short_name.remove(".so");
    return short_name;
}

void MFDPluginWrapper::log(const QString &log_entry, int verbosity)
{
    LoggingEvent *le = new LoggingEvent(log_entry, verbosity);
    QApplication::postEvent(parent, le);
}

void MFDPluginWrapper::warning(const QString &warning_text)
{
    QString warn = warning_text;
    warn.prepend("WARNING: ");
    LoggingEvent *le = new LoggingEvent(warn, 1);
    QApplication::postEvent(parent, le);
}

bool MFDPluginWrapper::tryToUnload()
{
    if(can_unload_function)
    {
        //
        //  If the plugin is really done ...
        //

        if(can_unload_function())
        {

            //
            //  ... kill it off
            //

            if(unload())
            {
                return true;
            }
        }
        else
        {
            //
            //  otherwise, keep telling it to stop(). We may have to tell it
            //  many times, as stop() does not block.
            //

            if(stop_function)
            {
                stop_function();
            }
        }
    }
    return false;
}

void MFDPluginWrapper::metadataChanged(int which_collection, bool external)
{
    if(metadata_change_function)
    {
        metadata_change_function(which_collection, external);
    }
}

void MFDPluginWrapper::servicesChanged()
{
    if(services_change_function)
    {
        services_change_function();
    }
}

/*
---------------------------------------------------------------------
*/

MFDPluginManager::MFDPluginManager(MFD *owner)
{

    //
    //  Set some defaults
    //

    unique_plugin_identifier = 0;
    available_plugins.clear();
    dead_plugins.clear();
    parent = owner;
    lib_open_daap = NULL;
    generateHashFunction = NULL;
    daap_request_id = 0;
    
    //
    //  Start a timer to check for dead 
    //  plugins.
    //
    
    dead_plugin_timer = new QTimer();
    connect(dead_plugin_timer, SIGNAL(timeout()),
           this, SLOT(cleanDeadPlugins()));
    dead_plugin_timer->start(10000); // 10 seconds, in shutdown process, this gets set faster

    //
    //  Do initial plugin load
    //

    very_first_time = true;
    shutdown_flag = false;
    cleanDeadPlugins();
}

void MFDPluginManager::reloadPlugins()
{
    //
    //  This stops current plugins. The cleaning
    //  thread figures out when they're *all* gone, and
    //  only then asks for them to be reloaded (by
    //  calling afterStopLoad() ). Note that a plugin
    //  that has called fatal() will *not* be reloaded.
    //    

    log("plugin manager is (re-)loading plugins", 1);
    stopPlugins();
}
    
void MFDPluginManager::afterStopLoad()
{    
    //
    //  If we can, find libopendaap
    //
    
    if(!lib_open_daap)
    {
        lib_open_daap = new QLibrary("opendaap");
        if(lib_open_daap->load())
        {
            //
            //  Well, the library is there ... get a reference to the
            //  GenerateHash() function (the magic bit of code that knows how to
            //  do Apple Daap client validation headers)
            //

            generateHashFunction = (Generate_Hash_Function) 
                     lib_open_daap->resolve("GenerateHash");

            if (!generateHashFunction )
            {
                warning("libopendaap was loaded, but could not "
                        "resolve GenerateHash()");
            }
            else
            {
                log("libopendaap loaded, will be used for client daap validation (iTunes)", 2);
            }
        
        }
        else
        {
            log("did not find libopendaap, no talking to iTunes", 2);
            delete lib_open_daap;
            lib_open_daap = NULL;
            generateHashFunction = NULL;
        }
    }

    plugin_file_list.clear();
    QString where_to_look = QDir::currentDirPath() + "/plugins/";
    buildPluginFileList(where_to_look);
    if(plugin_file_list.count() < 1)
    {
        QString installed_plugins = QString(PREFIX) + "/lib/mythtv/mfdplugins/"; 
        log(QString("plugin manager is going to load plugins from %1").arg(installed_plugins), 2);
        buildPluginFileList(installed_plugins);
    }
    else
    {
        log("plugin manager is going to load plugins below working directory", 2);
    }

    if(plugin_file_list.count() < 1)
    {
        warning("plugin manager found no plugins (not even dummy)");
    }        
    else
    {
        //
        //  Actually load the plugins
        //
        
        for(uint i = 0; i < plugin_file_list.count(); i++)
        {
            MFDPluginWrapper *new_plugin = new MFDPluginWrapper(parent, plugin_file_list[i], bumpIdentifier());
            
            //
            //  If it's on the skip list (previous fatal() call), ignore it.
            //
            
            if(plugin_skip_list.contains(new_plugin->shortName()))
            {
                delete new_plugin;
                new_plugin = NULL;
            }
            
            if(new_plugin)
            {
                if(new_plugin->init())
                {
                    //
                    //  Yay, it's a "valid" mfd plugin that behaves as it should
                    //
                
                    log(QString("plugin manager loaded plugin %1 (unique identifier is %2)")
                        .arg(new_plugin->shortName())
                        .arg(new_plugin->getIdentifier()), 2);
                    new_plugin->getCapabilities();
                    if(!new_plugin->run())
                    {
                        warning(QString("plugin manager could not get the %1 plugin to run so will unload it")
                                .arg(new_plugin->shortName()));
                        new_plugin->unload();
                        delete new_plugin;
                    }
                    else
                    {
                        available_plugins.append(new_plugin);
                    }
                }
                else
                {
                    warning(QString("plugin manager was unable to load this plugin: %1").arg(new_plugin->shortName()));
                    new_plugin->unload();
                    delete new_plugin;
                }
            }
        }
    }

    emit allPluginsLoaded();
}

void MFDPluginManager::stopPlugin(int which_one)
{
    QPtrListIterator<MFDPluginWrapper> iterator( available_plugins );
    MFDPluginWrapper *a_plugin;
    while ( (a_plugin = iterator.current()) != 0 )
    {
        ++iterator;
        if(a_plugin->getIdentifier() == which_one)
        {
            plugin_skip_list.append(a_plugin->shortName());
            log(QString("plugin manager is asking only the following plugin to stop: %1").arg(a_plugin->shortName()), 2);
            a_plugin->stop();
            available_plugins.remove(a_plugin);
            dead_plugins.append(a_plugin);
            break;
        }
    }
}

void MFDPluginManager::tellPluginsMetadataChanged(int which_collection, bool external)
{
    QPtrListIterator<MFDPluginWrapper> iterator( available_plugins );
    MFDPluginWrapper *a_plugin;
    while ( (a_plugin = iterator.current()) != 0 )
    {
        ++iterator;
        a_plugin->metadataChanged(which_collection, external);
    }
}

void MFDPluginManager::tellPluginsServicesChanged()
{
    QPtrListIterator<MFDPluginWrapper> iterator( available_plugins );
    MFDPluginWrapper *a_plugin;
    while ( (a_plugin = iterator.current()) != 0 )
    {
        ++iterator;
        a_plugin->servicesChanged();
    }
}

void MFDPluginManager::stopPlugins()
{

    QPtrListIterator<MFDPluginWrapper> iterator( available_plugins );
    MFDPluginWrapper *a_plugin;
    while ( (a_plugin = iterator.current()) != 0 )
    {
        ++iterator;
        log(QString("plugin manager is asking the following plugin to stop: %1").arg(a_plugin->shortName()), 2);
        a_plugin->stop();
        available_plugins.remove(a_plugin);
        dead_plugins.append(a_plugin);
    }

    if(available_plugins.count() > 0)
    {
        warning("plugin manager failed to stop all plugins in stopPlugins()");
    }    
}

void MFDPluginManager::buildPluginFileList(QString directory)
{
    //
    //  Recursively build a list of files
    //  that match the patten of a mfd
    //  plugin 
    //

    QDir d(directory);

    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || 
            fi->fileName() == ".." )
        {
            continue;
        }
        
        QString filename = fi->absFilePath();
        if (fi->isDir())
        {
            buildPluginFileList(filename);
        }
        else
        {
            QString base_filename = filename.section('/', -1);
            if( base_filename.left(13) == "libmfdplugin-" &&
                base_filename.right(3) == ".so")
            {
                plugin_file_list.append(filename);
            }
        }
    }
}

bool MFDPluginManager::parseTokens(const QStringList &tokens, int socket_identifier)
{
    if(tokens.count() < 1)
    {
        return false;
    }

    log(QString("plugin manager was asked to see if it can parse the following message: %1")
        .arg(tokens.join(" ")), 10);
    
    //
    //  Pass tokens and socket pointer to
    //  any and *all* plugin(s) that will take it
    //
    
    bool something_took_it = false;
    QPtrListIterator<MFDPluginWrapper> iterator(available_plugins);
    MFDPluginWrapper *a_plugin;
    while ( (a_plugin = iterator.current()) != 0 )
    {
        ++iterator;
        if(a_plugin->acceptsToken(tokens[0]))
        {
            a_plugin->parseTokens(tokens, socket_identifier);
            something_took_it = true;
        }
    }
    
    if(something_took_it)
    {
        return true;
    }
    return false;
}

void MFDPluginManager::log(const QString &log_entry, int verbosity)
{
    LoggingEvent *le = new LoggingEvent(log_entry, verbosity);
    QApplication::postEvent(parent, le);
}

void MFDPluginManager::warning(const QString &warning_text)
{
    QString warn = warning_text;
    warn.prepend("WARNING: ");
    LoggingEvent *le = new LoggingEvent(warn, 1);
    QApplication::postEvent(parent, le);
}

void MFDPluginManager::sendMessage(const QString &message, int socket_identifier)
{
    SocketEvent *se = new SocketEvent(message, socket_identifier);
    QApplication::postEvent(parent, se);    
}

bool MFDPluginManager::pluginsExist()
{
    if(dead_plugins.count() > 0)
    {
        return true;
    }
    return false;
}

void MFDPluginManager::cleanDeadPlugins()
{
    bool possibly_load_after = false;

    if(very_first_time)
    {
        very_first_time = false;
        possibly_load_after = true;
    }
    
    QPtrListIterator<MFDPluginWrapper> iterator( dead_plugins );
    MFDPluginWrapper *a_plugin;
    while ( (a_plugin = iterator.current()) != 0 )
    {
        ++iterator;
        if(a_plugin->tryToUnload())
        {
            if(!plugin_skip_list.contains(a_plugin->shortName()))
            {
                possibly_load_after = true;
            }
            log(QString("plugin manager succeeded in removing a plugin called %1 (unique id %2)")
                .arg(a_plugin->shortName())
                .arg(a_plugin->getIdentifier()), 2);
            dead_plugins.remove(a_plugin);
            delete a_plugin;
        }
        else
        {
            log(QString("plugin manager did not succeed in removing a plugin called %1 (unique id %2) but will try again later")
                .arg(a_plugin->shortName())
                .arg(a_plugin->getIdentifier()), 7);
        }
    }
    if(dead_plugins.count() < 1 && possibly_load_after && !shutdown_flag)
    {
        //
        //  All dead plugins have been cleaned 
        //
        afterStopLoad();
    }
}


void MFDPluginManager::doListCapabilities(int socket_identifier)
{
    QPtrListIterator<MFDPluginWrapper> iterator( available_plugins );
    MFDPluginWrapper *a_plugin;
    while ( (a_plugin = iterator.current()) != 0 )
    {
        ++iterator;
        QStringList *tokens = a_plugin->getListOfCapabilities();
        for(uint i = 0; i < tokens->count(); i++)
        {
            sendMessage( QString("capability %1")
                        .arg((*tokens->at(i))), 
                        socket_identifier);
        }
    }
}

void MFDPluginManager::fillValidationHeader(
                                                int server_daap_version_major,
                                                const QString &request, 
                                                unsigned char *resulting_hash,
                                                int request_id,
                                                int hash_algorithm_version
                                            )
{
    hashing_mutex.lock();
        if(generateHashFunction)
        {
            generateHashFunction(
                                    server_daap_version_major,
                                    (const unsigned char *)request.ascii(),
                                    hash_algorithm_version,
                                    resulting_hash,
                                    request_id
                                );
        }
        else
        {
            resulting_hash[0] = '\0';
        }
    hashing_mutex.unlock();
}

bool MFDPluginManager::haveLibOpenDaap()
{
    if(lib_open_daap)
    {
        if(generateHashFunction)
        {
            return true;
        }
    }
    return false;
}


int MFDPluginManager::bumpDaapRequestId()
{
    int return_value;
    daap_request_id_mutex.lock();
        ++daap_request_id;
        return_value = daap_request_id;
    daap_request_id_mutex.unlock();
    return return_value;
}

//
//  This is only for debugging
//

void MFDPluginManager::printPlugins()
{
    cout << endl << endl;
    cout << "available plugins are: " << endl;
    for (uint i = 0; i < available_plugins.count(); i++)
    {
        cout << "       " 
             << available_plugins.at(i)->shortName() 
             << " (" 
             << available_plugins.at(i)->getIdentifier()
             << ")"
             << endl;
    }
    cout << endl;
    cout << "dead plugins are: " << endl;
    for (uint i = 0; i < dead_plugins.count(); i++)
    {
        cout << "       " 
             << dead_plugins.at(i)->shortName() 
             << " (" 
             << dead_plugins.at(i)->getIdentifier()
             << ")"
             << endl;
    }
    cout << endl;
}

void MFDPluginManager::setDeadPluginTimer(int an_int)
{
    if(dead_plugin_timer)
    {
        dead_plugin_timer->changeInterval(an_int);
    }
    else
    {
        cerr << "can't set dead plugin timer because there isn't one" << endl;
    }
}

MFDPluginManager::~MFDPluginManager()
{
    if(dead_plugin_timer)
    {
        delete dead_plugin_timer;
    }
    if(lib_open_daap)
    {
        delete lib_open_daap;
        lib_open_daap = NULL;
    }
}


