#ifndef PLUGINMANAGER_H_
#define PLUGINMANAGER_H_
/*
	pluginmanager.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the plugin manager and wrapper

*/


#include <qlibrary.h>
#include <qstringlist.h>
#include <qsocket.h>
#include <qtimer.h>
#include <qmutex.h>

class MFD;


class MFDPluginWrapper : public QLibrary
{
    //
    //  This class wraps the loaded plugins
    //  in an "easy to use" interface.
    //
    
  public:
  
    MFDPluginWrapper(MFD *owner, const QString &file_name, int identifier);
    bool init();
    void getCapabilities();
    bool run();
    void parseTokens(const QStringList &tokens, int socket_identifier);
    void stop();
    bool acceptsToken(const QString &a_token);    
    const QString& shortName();
    int  getIdentifier(){return unique_identifier;}
    bool tryToUnload();
    void metadataChanged(int which_collection, bool external);
    void servicesChanged();

    QStringList* getListOfCapabilities(){return &acceptable_tokens;}
    
  private:
  
    void log(const QString &log_entry, int verbosity);
    void warning(const QString &warning_text);

    MFD         *parent;
    QString     short_name;
    QStringList acceptable_tokens;
    int         unique_identifier;

    //
    //  Interfaces to the plugin
    //  (I barely understand what I'm doing here)
    //
    
    typedef bool (*Plugin_Init_Function)(MFD*, int);
    Plugin_Init_Function init_function;

    typedef QStringList (*Plugin_Capabilities_Function)();
    Plugin_Capabilities_Function capabilities_function;
    
    typedef bool (*Plugin_Run_Function)();
    Plugin_Run_Function run_function;
    
    typedef void (*Plugin_Parse_Tokens_Function)(const QStringList &, int);
    Plugin_Parse_Tokens_Function parse_tokens_function;

    typedef void (*Plugin_Metadata_Change_Function)(int, bool);
    Plugin_Metadata_Change_Function metadata_change_function;

    typedef void (*Plugin_Services_Change_Function)();
    Plugin_Services_Change_Function services_change_function;

    typedef bool (*Plugin_Stop_Function)();
    Plugin_Stop_Function stop_function;

    typedef bool (*Plugin_Can_Unload_Function)();
    Plugin_Can_Unload_Function can_unload_function;
};



class MFDPluginManager : public QObject
{

  Q_OBJECT
  
  public:

    MFDPluginManager(MFD *owner);
    ~MFDPluginManager();

    bool parseTokens(const QStringList &tokens, int socket_identifier);
    void doListCapabilities(int socket_identifier);
    void reloadPlugins();
    void afterStopLoad();

    void stopPlugin(int which_one);
    void stopPlugins();

    bool pluginsExist();
    
    void tellPluginsMetadataChanged(int which_collection, bool external);
    void tellPluginsServicesChanged();
    
    void setShutdownFlag(bool true_or_false){shutdown_flag = true_or_false;}
    void setDeadPluginTimer(int an_int);

    //
    //  Bit odd to have this sitting here, but it has to go somewhere that
    //  is accesible by all the plugins
    //

    void fillValidationHeader(
                                int server_daap_version_major,
                                const QString &request, 
                                unsigned char *resulting_hash,
                                int request_id,
                                int hash_algorithm_version = 2
                             );
    bool haveLibOpenDaap();
    int  bumpDaapRequestId();
    
    //
    //  debugging
    //
    
    void printPlugins();

  signals:
  
    void allPluginsLoaded();

  private slots:
  
    void cleanDeadPlugins();
    
  private:

    void buildPluginFileList(QString directory);
    void log(const QString &log_entry, int verbosity);
    void warning(const QString &warning_text);
    void sendMessage(const QString &message, int socket_identifier);
    int  bumpIdentifier(){++unique_plugin_identifier; return unique_plugin_identifier;}

    MFD                        *parent;   
    QPtrList<MFDPluginWrapper> available_plugins;
    QPtrList<MFDPluginWrapper> dead_plugins;
    QStringList                plugin_file_list;
    QStringList                plugin_skip_list;
    int                        unique_plugin_identifier;
    QTimer                     *dead_plugin_timer;
    bool                       very_first_time;
    bool                       shutdown_flag;

    //
    //  One outside library that different plugins may want to use if it is
    //  (dynamically) available
    //

    QLibrary                   *lib_open_daap;
    typedef void (*Generate_Hash_Function)
            (
                short, 
                const unsigned char*, 
                unsigned char, 
                unsigned char*,
                int
            );
    Generate_Hash_Function generateHashFunction;
    QMutex hashing_mutex;

    //
    //  And since plugin_manager is canonical place for DAAP validation, it
    //  should also be the place to hand out unique DAAP request id's
    //
    
    QMutex  daap_request_id_mutex;
    int     daap_request_id;
};



#endif

