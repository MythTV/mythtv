// C includes
#include <dlfcn.h>

// Qt includes
#include <qstringlist.h>
#include <qdict.h>
#include <qdir.h>

// MythTV includes
#include "mythplugin.h"
#include "mythcontext.h"
#include "langsettings.h"

using namespace std;

MythPlugin::MythPlugin(const QString &libname)
          : QLibrary(libname)
{
    enabled = true;
    position = 0;
}

MythPlugin::~MythPlugin()
{
}

int MythPlugin::init(const char *libversion)
{
    typedef int (*PluginInitFunc)(const char *);
    PluginInitFunc ifunc = (PluginInitFunc)QLibrary::resolve("mythplugin_init");

    if (ifunc)
        return ifunc(libversion);

    VERBOSE(VB_IMPORTANT, QString("MythPlugin::Init() dlerror: %1").arg(dlerror()));
    return -1;
}

void MythPlugin::run(void)
{
    typedef int (*PluginRunFunc)();
    PluginRunFunc rfunc = (PluginRunFunc)QLibrary::resolve("mythplugin_run");

    if (rfunc)
        rfunc();
}

void MythPlugin::config(void)
{
    typedef int (*PluginConfigFunc)();
    PluginConfigFunc rfunc = (PluginConfigFunc)QLibrary::resolve("mythplugin_config");

    if (rfunc)
        rfunc();
}

MythPluginType MythPlugin::type(void)
{
    typedef MythPluginType (*PluginTypeFunc)();
    PluginTypeFunc rfunc = (PluginTypeFunc)QLibrary::resolve("mythplugin_type");

    if (rfunc)
        return rfunc();

    return kPluginType_Module;
}

int MythPlugin::setupMenuPlugin(void)
{
    typedef int (*PluginSetup)();
    PluginSetup rfunc = (PluginSetup)QLibrary::resolve("mythplugin_setupMenu");

    if (rfunc)
        return rfunc();

    return -1;
}

void MythPlugin::drawMenuPlugin(QPainter *painter, int x, int y, int w, int h)
{
    typedef void (*PluginDrawMenu)(QPainter *, int, int, int, int);
    PluginDrawMenu rfunc = (PluginDrawMenu)QLibrary::resolve("mythplugin_drawMenu");

    if (rfunc)
        rfunc(painter, x, y, w, h);
}

MythPluginManager::MythPluginManager()
{
    m_dict.setAutoDelete(true);

    QString pluginprefix = gContext->GetPluginsDir();

    QDir filterDir(pluginprefix);

    filterDir.setFilter(QDir::Files | QDir::Readable);
    QString filter = gContext->GetPluginsNameFilter();
    filterDir.setNameFilter(filter);

    gContext->SetDisableLibraryPopup(true);

    if (filterDir.exists())
    {
        int prefixLength = filter.find("*");
        int suffixLength = filter.length() - prefixLength - 1;

        QStringList libraries = filterDir.entryList();
        for (QStringList::iterator i = libraries.begin(); i != libraries.end();
             i++)
        {
            QString library = *i;

            // pull out the base library name
            library = library.right(library.length() - prefixLength);
            library = library.left(library.length() - suffixLength);

            init_plugin(library);
        }
    }

    gContext->SetDisableLibraryPopup(false);

    orderMenuPlugins();
}

bool MythPluginManager::init_plugin(const QString &plugname)
{
    QString newname = gContext->FindPlugin(plugname);
   
    if (m_dict.find(newname) == 0)
    {
        m_dict.insert(newname, new MythPlugin(newname));
        m_dict[newname]->setAutoUnload(true);
    }
   
    int result = m_dict[newname]->init(MYTH_BINARY_VERSION);
  
    if (result == -1)
    {
        m_dict.remove(newname);
        VERBOSE(VB_IMPORTANT, QString("Unable to initialize plugin '%1'.")
                .arg(plugname));
        return false;
    }
    
    LanguageSettings::load(plugname);

    switch (m_dict[newname]->type())
    {
        case kPluginType_MenuPlugin:
            menuPluginMap[newname] = m_dict[newname];
            break;
        case kPluginType_Module:
        default:
            moduleMap[newname] = m_dict[newname];
            break;
    }

    return true;
}

bool MythPluginManager::run_plugin(const QString &plugname)
{
    QString newname = gContext->FindPlugin(plugname);

    if (m_dict.find(newname) == 0 && init_plugin(plugname) == false)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to run plugin '%1': not initialized")
                .arg(plugname));
        return false;
    }

    m_dict[newname]->run();
    return true;
}

bool MythPluginManager::config_plugin(const QString &plugname)
{
    QString newname = gContext->FindPlugin(plugname);

    if (m_dict.find(newname) == 0 && init_plugin(plugname) == false)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to configure plugin '%1': not initialized")
                .arg(plugname));
        return false;
    }

    m_dict[newname]->config();
    return true;
}

MythPlugin *MythPluginManager::GetPlugin(const QString &plugname)
{
    QString newname = gContext->FindPlugin(plugname);

    if (moduleMap.find(newname) == moduleMap.end())
        return NULL;

    return moduleMap[newname];
}

MythPlugin *MythPluginManager::GetMenuPlugin(const QString &plugname)
{
    QString newname = gContext->FindPlugin(plugname);

    if (menuPluginMap.find(newname) == menuPluginMap.end())
        return NULL;

    return menuPluginMap[newname];
}

MythPlugin *MythPluginManager::GetMenuPluginAt(int pos)
{
    if (pos >= (int)menuPluginList.count())
        return NULL;

    return menuPluginList.at(pos);
}

void MythPluginManager::orderMenuPlugins(void)
{
    // This needs to hit a db table for persistant ordering
    // For now, just use whatever order the map iterator returns

    menuPluginList.clear();

    QMap<QString, MythPlugin *>::iterator iter = menuPluginMap.begin();
    for (; iter != menuPluginMap.end(); ++iter)
    {
        if (iter.data()->isEnabled())
            menuPluginList.append(iter.data());
    }
}

