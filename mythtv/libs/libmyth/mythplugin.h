#ifndef MYTHPLUGIN_H_
#define MYTHPLUGIN_H_

#include <qlibrary.h>
#include <qdict.h>
#include <qmap.h>
#include <qptrlist.h>

class QSqlDatabase;
class MythContext;
class QPainter;

typedef enum {
    kPluginType_Module = 0,
    kPluginType_MenuPlugin
} MythPluginType;

class MythPlugin : public QLibrary
{
  public:
    MythPlugin(const QString &);
    virtual ~MythPlugin();

    // This method will call the mythplugin_init() function of the library. 
    int init(const char *libversion);

    // This method will call the mythplugin_run() function of the library.
    void run(void);
 
    // This method will call the mythplugin_config() function of the library.
    void config(void);

    // This method will call the mythplugin_type() function of the library.
    // If such a function doesn't exist, it's a main module plugin.
    MythPluginType type(void);

    // This method will call the mythplugin_destroy() function of the library,
    // if such a function exists.
    void destroy(void);

    bool isEnabled() { return enabled; }
    void setEnabled(bool enable) { enabled = enable; }

    int getPosition() { return position; }
    void setPosition(int pos) { position = pos; }

    // mainmenu plugins, probably should separate out

    // setup the plugin -- returns how often (in ms) the plugin wants updated
    int setupMenuPlugin(void);

    // draw the plugin
    void drawMenuPlugin(QPainter *painter, int x, int y, int w, int h);

  private:
    bool enabled;
    int position;
};

// this should only be instantiated through MythContext.
class MythPluginManager
{
  public:   
    MythPluginManager();
   ~MythPluginManager();
   
    bool init_plugin(const QString &plugname);
    bool run_plugin(const QString &plugname);
    bool config_plugin(const QString &plugname);
    bool destroy_plugin(const QString &plugname);

    MythPlugin *GetPlugin(const QString &plugname);
    MythPlugin *GetMenuPlugin(const QString &plugname);
    MythPlugin *GetMenuPluginAt(int pos);

    void DestroyAllPlugins();
     
  private:
    QDict<MythPlugin> m_dict;
   
    QMap<QString, MythPlugin *> moduleMap;
    QMap<QString, MythPlugin *> menuPluginMap;
    QPtrList<MythPlugin> menuPluginList;

    void orderMenuPlugins();
};

#endif

