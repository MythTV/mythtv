#ifndef MYTHPLUGIN_H_
#define MYTHPLUGIN_H_

#include <qlibrary.h>
#include <qdict.h>

class QSqlDatabase;
class MythContext;

class MythPluginManager
{
  public:
    class MythPlugin : public QLibrary
    {
      public:
        MythPlugin (const QString &);
        virtual ~MythPlugin ();

        /*
         * This method will call the plugin_init() function
         * of the library.  The plugin_init() function should
         * be declared 'extern "C" { ... }'
         * Also, you may want to include a static local variable
         * inside the plugin_init() function so that initialization
         * is only done the first time the library is loaded.
         */
        int init(const char *libversion);

        /*
         * This method will call the plugin_run() function
         * of the library.  The plugin_run() function should
         * be declared 'extern "C" { ... }'
         * Whether or not this function returns immediately
         * is entirely dependent on the design of the plugin.
         * You should assume that it will not return until
         * the plugin wants to return control back to the caller.
         */
         void run(void);

        /* 
         * This method will call the plugin_config() function
         * of the library.  The plugin_config() function should
         * be declared 'extern "C" { ... }'
         */
         void config(void);
    };
      
    static void init(void);
    static bool init_plugin(const QString &);
    static void run_plugin(const QString &);
    static void config_plugin(const QString &);
    static void unload_plugin(const QString &);
     
  private:
    static QDict<MythPlugin> m_dict;
   
    // Default constructor declared private and
    // left as abstract to prevent instantiation
    MythPluginManager();
};

#endif

