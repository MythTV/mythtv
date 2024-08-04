#ifndef MYTHPLUGIN_H_
#define MYTHPLUGIN_H_

#include <utility>

// Qt headers
#include <QHash>
#include <QLibrary>
#include <QMap>

// MythTV headers
#include "mythbaseexp.h"

class QSqlDatabase;
class MythContext;
class QPainter;

enum MythPluginType : std::uint8_t {
    kPluginType_Module = 0
};

class MythPlugin : public QLibrary
{
  public:
    MythPlugin(const QString &libname, QString plugname)
        : QLibrary(libname), m_plugName(std::move(plugname)) {}
    ~MythPlugin() override = default;

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

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enable) { m_enabled = enable; }

    int getPosition() const { return m_position; }
    void setPosition(int pos) { m_position = pos; }

    QString getName(void) { return m_plugName; }

  private:
    bool m_enabled {true};
    int m_position {0};
    QString m_plugName;
    QStringList m_features;
};

// this should only be instantiated through MythContext.
class MBASE_PUBLIC MythPluginManager
{
  public:
    MythPluginManager();
   ~MythPluginManager() = default;

    bool init_plugin(const QString &plugname);
    bool run_plugin(const QString &plugname);
    bool config_plugin(const QString &plugname);
    bool destroy_plugin(const QString &plugname);

    MythPlugin *GetPlugin(const QString &plugname);

    QStringList EnumeratePlugins(void);
    void DestroyAllPlugins();

  private:
    QHash<QString,MythPlugin*> m_dict;

    QMap<QString, MythPlugin *> m_moduleMap;
};

#endif
