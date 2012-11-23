#ifndef MYTHPLUGIN_H_
#define MYTHPLUGIN_H_

#include <QLibrary>
#include <QMap>
#include <QHash>

#include <vector>
using namespace std;

#include "mythbaseexp.h"

class QSqlDatabase;
class MythContext;
class QPainter;

typedef enum {
    kPluginType_Module = 0
} MythPluginType;

class MythPlugin : public QLibrary
{
  public:
    MythPlugin(const QString &, const QString &);
    virtual ~MythPlugin();

    // This method will call the mythplugin_init() function of the library. 
    int init(const char *libversion);

    // This method will call the mythplugin_run() function of the library.
    int run(void);
 
    // This method will call the mythplugin_config() function of the library.
    int config(void);

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

    QString getName(void) { return m_plugName; }

  private:
    bool enabled;
    int position;
    QString m_plugName;
    QStringList m_features;
};

// this should only be instantiated through MythContext.
class MBASE_PUBLIC MythPluginManager
{
  public:   
    MythPluginManager();
   ~MythPluginManager();
   
    bool init_plugin(const QString &plugname);
    bool run_plugin(const QString &plugname);
    bool config_plugin(const QString &plugname);
    bool destroy_plugin(const QString &plugname);

    MythPlugin *GetPlugin(const QString &plugname);

    QStringList EnumeratePlugins(void);
    void DestroyAllPlugins();
     
  private:
    QHash<QString,MythPlugin*> m_dict;
   
    QMap<QString, MythPlugin *> moduleMap;
};

#endif

