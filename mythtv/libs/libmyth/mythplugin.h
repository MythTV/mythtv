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

class MythPluginManager
{
  public:      
    static void init(void);
    static bool init_plugin(const QString &plugname);
    static bool run_plugin(const QString &plugname);
    static bool config_plugin(const QString &plugname);
    static void unload_plugin(const QString &plugname);

    static MythPlugin *GetPlugin(const QString &plugname);
    static MythPlugin *GetMenuPlugin(const QString &plugname);
    static MythPlugin *GetMenuPluginAt(int pos);
     
  private:
    static QDict<MythPlugin> m_dict;
   
    static QMap<QString, MythPlugin *> moduleMap;
    static QMap<QString, MythPlugin *> menuPluginMap;
    static QPtrList<MythPlugin> menuPluginList;

    // Default constructor declared private and
    // left as abstract to prevent instantiation
    MythPluginManager();

    static void orderMenuPlugins();
};

#endif

