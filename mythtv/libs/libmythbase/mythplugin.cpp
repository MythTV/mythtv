// C includes
#ifndef _WIN32
#include <dlfcn.h>
#else
#include "compat.h"
#endif

// Qt includes
#include <QDir>

// MythTV includes

// libmythbase
#include "mythplugin.h"
#include "mythcorecontext.h"
#include "mythtranslation.h"
#include "mythdirs.h"
#include "mythversion.h"
#include "mythlogging.h"

int MythPlugin::init(const char *libversion)
{
    using PluginInitFunc = int (*)(const char *);
    auto ifunc = (PluginInitFunc)QLibrary::resolve("mythplugin_init");
    if (ifunc)
        return ifunc(libversion);

    QString error_msg(dlerror());
    if (error_msg.isEmpty())
    {
        QByteArray libname = QLibrary::fileName().toLatin1();
        (void)dlopen(libname.constData(), RTLD_LAZY);
        error_msg = dlerror();
    }

    LOG(VB_GENERAL, LOG_EMERG, QString("MythPlugin::init() dlerror: %1")
            .arg(error_msg));

    return -1;
}

int MythPlugin::run(void)
{
    using PluginRunFunc = int (*)();

    int rVal = -1;
    auto rfunc = (PluginRunFunc)QLibrary::resolve("mythplugin_run");

    if (rfunc)
        rVal = rfunc();

    return rVal;
}

int MythPlugin::config(void)
{
    using PluginConfigFunc = int (*)();

    int rVal  = -1;
    auto rfunc = (PluginConfigFunc)QLibrary::resolve("mythplugin_config");

    if (rfunc)
    {
        rVal = rfunc();
        gCoreContext->ClearSettingsCache();
    }

    return rVal;
}

MythPluginType MythPlugin::type(void)
{
    using PluginTypeFunc = MythPluginType (*)();
    auto rfunc = (PluginTypeFunc)QLibrary::resolve("mythplugin_type");

    if (rfunc)
        return rfunc();

    return kPluginType_Module;
}

void MythPlugin::destroy(void)
{
    using PluginDestFunc = void (*)();
    PluginDestFunc rfunc = QLibrary::resolve("mythplugin_destroy");

    if (rfunc)
        rfunc();
}

MythPluginManager::MythPluginManager()
{
    QString pluginprefix = GetPluginsDir();

    QDir filterDir(pluginprefix);

    filterDir.setFilter(QDir::Files | QDir::Readable);
    QString filter = GetPluginsNameFilter();
    filterDir.setNameFilters(QStringList(filter));

    if (filterDir.exists())
    {
        int prefixLength = filter.indexOf("*");
        int suffixLength = filter.length() - prefixLength - 1;

        QStringList libraries = filterDir.entryList();
        if (libraries.isEmpty())
            LOG(VB_GENERAL, LOG_WARNING,
                    "No libraries in plugins directory " + filterDir.path());

        // coverity[auto_causes_copy]
        for (auto library : std::as_const(libraries))
        {
            // pull out the base library name
            library = library.right(library.length() - prefixLength);
            library = library.left(library.length() - suffixLength);

            init_plugin(library);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING,
                 "No plugins directory " + filterDir.path());
    }
}

bool MythPluginManager::init_plugin(const QString &plugname)
{
    QString newname = FindPluginName(plugname);

    if (!m_dict[newname])
    {
        m_dict.insert(newname, new MythPlugin(newname, plugname));
    }

    int result = m_dict[newname]->init(MYTH_BINARY_VERSION);

    if (result == -1)
    {
        delete m_dict[newname];
        m_dict.remove(newname);
        LOG(VB_GENERAL, LOG_ERR,
                 QString("Unable to initialize plugin '%1'.") .arg(plugname));
        return false;
    }

    MythTranslation::load(plugname);

    switch (m_dict[newname]->type())
    {
        case kPluginType_Module:
        default:
            m_moduleMap[newname] = m_dict[newname];
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
        LOG(VB_GENERAL, LOG_ALERT,
                 QString("Unable to run plugin '%1': not initialized")
                     .arg(plugname));
        return true;
    }

    bool res = m_dict[newname]->run() != 0;

    return res;
}

// return false on success, true on error
bool MythPluginManager::config_plugin(const QString &plugname)
{
    QString newname = FindPluginName(plugname);

    if (!m_dict[newname] && !init_plugin(plugname))
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 QString("Unable to configure plugin '%1': not initialized")
                     .arg(plugname));
        return true;
    }

    bool res = m_dict[newname]->config() != 0;

    return res;
}

bool MythPluginManager::destroy_plugin(const QString &plugname)
{
    QString newname = FindPluginName(plugname);

    if (!m_dict[newname] && !init_plugin(plugname))
    {
        LOG(VB_GENERAL, LOG_ALERT,
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

    if (m_moduleMap.find(newname) == m_moduleMap.end())
        return nullptr;

    return m_moduleMap[newname];
}

void MythPluginManager::DestroyAllPlugins(void)
{
    for (auto *it : std::as_const(m_dict))
    {
        it->destroy();
        delete it;
    }

    m_dict.clear();
    m_moduleMap.clear();
}

QStringList MythPluginManager::EnumeratePlugins(void)
{
    QStringList ret;
    for (auto *it : std::as_const(m_dict))
        ret << it->getName();
    return ret;
}
