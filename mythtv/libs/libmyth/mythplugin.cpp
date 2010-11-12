// C includes
#ifndef USING_MINGW
#include <dlfcn.h>
#else
#include "compat.h"
#endif

// Qt includes
#include <QDir>

// MythTV includes
#include "mythplugin.h"
#include "mythcontext.h"
#include "mythtranslation.h"

#include "mythdirs.h"
#include "mythversion.h"
#include "mythverbose.h"

using namespace std;

MythPlugin::MythPlugin(const QString &libname)
          : QLibrary(libname)
{
    enabled = true;
    position = 0;
}

MythPlugin::~MythPlugin()
{
    // Commented out because it causes segfaults... dtk 2008-10-08
    //if (isLoaded())
    //    unload();
}

int MythPlugin::init(const char *libversion)
{
    typedef int (*PluginInitFunc)(const char *);
    PluginInitFunc ifunc = (PluginInitFunc)QLibrary::resolve("mythplugin_init");

    if (ifunc)
        return ifunc(libversion);

    QString error_msg(dlerror());
    if (error_msg.isEmpty())
    {
        QByteArray libname = QLibrary::fileName().toAscii();
        (void)dlopen(libname.constData(), RTLD_LAZY);
        error_msg = dlerror();
    }

    VERBOSE(VB_IMPORTANT, QString("MythPlugin::init() dlerror: %1")
            .arg(error_msg));

    return -1;
}

int MythPlugin::run(void)
{
    typedef int (*PluginRunFunc)();

    int           rVal  = -1;
    PluginRunFunc rfunc = (PluginRunFunc)QLibrary::resolve("mythplugin_run");

    if (rfunc)
        rVal = rfunc();

    return rVal;
}

int MythPlugin::config(void)
{
    typedef int (*PluginConfigFunc)();

    int              rVal  = -1;
    PluginConfigFunc rfunc = (PluginConfigFunc)QLibrary::resolve(
                                                   "mythplugin_config");

    if (rfunc)
    {
        rVal = rfunc();
        gCoreContext->ClearSettingsCache();
    }

    return rVal;
}

MythPluginType MythPlugin::type(void)
{
    typedef MythPluginType (*PluginTypeFunc)();
    PluginTypeFunc rfunc = (PluginTypeFunc)QLibrary::resolve("mythplugin_type");

    if (rfunc)
        return rfunc();

    return kPluginType_Module;
}

void MythPlugin::destroy(void)
{
    typedef void (*PluginDestFunc)();
    PluginDestFunc rfunc = (PluginDestFunc)QLibrary::resolve("mythplugin_destroy");

    if (rfunc)
        rfunc();
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
    QString pluginprefix = GetPluginsDir();

    QDir filterDir(pluginprefix);

    filterDir.setFilter(QDir::Files | QDir::Readable);
    QString filter = GetPluginsNameFilter();
    filterDir.setNameFilters(QStringList(filter));

    gContext->SetDisableLibraryPopup(true);

    if (filterDir.exists())
    {
        int prefixLength = filter.indexOf("*");
        int suffixLength = filter.length() - prefixLength - 1;

        QStringList libraries = filterDir.entryList();
        if (libraries.isEmpty())
            VERBOSE(VB_GENERAL,
                    "No libraries in plugins directory " + filterDir.path());

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
    else
        VERBOSE(VB_GENERAL, "No plugins directory " + filterDir.path());

    gContext->SetDisableLibraryPopup(false);

    orderMenuPlugins();
}

MythPluginManager::~MythPluginManager()
{
}

bool MythPluginManager::init_plugin(const QString &plugname)
{
    QString newname = FindPluginName(plugname);

    if (!m_dict[newname])
    {
        m_dict.insert(newname, new MythPlugin(newname));
    }

    int result = m_dict[newname]->init(MYTH_BINARY_VERSION);

    if (result == -1)
    {
        delete m_dict[newname];
        m_dict.remove(newname);
        VERBOSE(VB_IMPORTANT, QString("Unable to initialize plugin '%1'.")
                .arg(plugname));
        return false;
    }

    MythTranslation::load(plugname);

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

// return false on success, true on error
bool MythPluginManager::run_plugin(const QString &plugname)
{
    QString newname = FindPluginName(plugname);

    if (!m_dict[newname] && !init_plugin(plugname))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to run plugin '%1': not initialized")
                .arg(plugname));
        return true;
    }

    bool res = m_dict[newname]->run();

    return res;
}

// return false on success, true on error
bool MythPluginManager::config_plugin(const QString &plugname)
{
    QString newname = FindPluginName(plugname);

    if (!m_dict[newname] && !init_plugin(plugname))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to configure plugin '%1': not initialized")
                .arg(plugname));
        return true;
    }

    bool res = m_dict[newname]->config();

    return res;
}

bool MythPluginManager::destroy_plugin(const QString &plugname)
{
    QString newname = FindPluginName(plugname);

    if (!m_dict[newname] && !init_plugin(plugname))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to destroy plugin '%1': not initialized")
                .arg(plugname));
        return false;
    }

    m_dict[newname]->destroy();
    return true;
}

MythPlugin *MythPluginManager::GetPlugin(const QString &plugname)
{
    QString newname = FindPluginName(plugname);

    if (moduleMap.find(newname) == moduleMap.end())
        return NULL;

    return moduleMap[newname];
}

MythPlugin *MythPluginManager::GetMenuPlugin(const QString &plugname)
{
    QString newname = FindPluginName(plugname);

    if (menuPluginMap.find(newname) == menuPluginMap.end())
        return NULL;

    return menuPluginMap[newname];
}

MythPlugin *MythPluginManager::GetMenuPluginAt(int pos)
{
    if ((uint)pos >= menuPluginList.size())
        return NULL;

    return menuPluginList[pos];
}

void MythPluginManager::orderMenuPlugins(void)
{
    // This needs to hit a db table for persistant ordering
    // For now, just use whatever order the map iterator returns

    menuPluginList.clear();

    QMap<QString, MythPlugin *>::iterator iter = menuPluginMap.begin();
    for (; iter != menuPluginMap.end(); ++iter)
    {
        if ((*iter)->isEnabled())
            menuPluginList.push_back(*iter);
    }
}

void MythPluginManager::DestroyAllPlugins(void)
{
    QHash<QString, MythPlugin*>::iterator it = m_dict.begin();
    for (; it != m_dict.end(); ++it)
    {
        (*it)->destroy();
        delete *it;
    }

    m_dict.clear();
    moduleMap.clear();
    menuPluginMap.clear();
    menuPluginList.clear();
}

