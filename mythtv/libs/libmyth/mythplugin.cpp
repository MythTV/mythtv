#include "mythplugin.h"
#include <qdict.h>
#include <qdir.h>
#include <iostream>
#include <dlfcn.h>

#include "mythcontext.h"

using namespace std;

MythPluginManager::MythPlugin::MythPlugin(const QString &libname)
                 : QLibrary(libname)
{
}

MythPluginManager::MythPlugin::~MythPlugin()
{
}

int MythPluginManager::MythPlugin::init(const char *libversion)
{
    typedef int (*PluginInitFunc)(const char *);
    PluginInitFunc ifunc = (PluginInitFunc)QLibrary::resolve("mythplugin_init");
	
    if (ifunc)
        return ifunc(libversion);

    cerr << dlerror() << endl;
    return -1;
}

void MythPluginManager::MythPlugin::run(void)
{
    typedef int (*PluginRunFunc)();	
    PluginRunFunc rfunc = (PluginRunFunc)QLibrary::resolve("mythplugin_run");
	
    if (rfunc)
        rfunc();
}

void MythPluginManager::MythPlugin::config(void)
{
    typedef int (*PluginConfigFunc)();
    PluginConfigFunc rfunc = (PluginConfigFunc)QLibrary::resolve("mythplugin_config");

    if (rfunc)
        rfunc();
}

QDict<MythPluginManager::MythPlugin> MythPluginManager::m_dict;

void MythPluginManager::init(void)
{
    m_dict.setAutoDelete(true);

    QString pluginprefix = QString(PREFIX) + "/lib/mythtv/plugins/";

    QDir filterDir(pluginprefix);

    filterDir.setFilter(QDir::Files | QDir::Readable);
    filterDir.setNameFilter("lib*.so");

    gContext->SetDisableLibraryPopup(true);

    if (filterDir.exists())
    {
        QStringList libraries = filterDir.entryList();
        for (QStringList::iterator i = libraries.begin(); i != libraries.end();
             i++)
        {
            QString library = *i;

            // pull out the base library name
            library = library.right(library.length() - 3);
            library = library.left(library.length() - 3);

            init_plugin(library);
        }
    }

    gContext->SetDisableLibraryPopup(false);
}

bool MythPluginManager::init_plugin(const QString & plugname)
{
    QString newname = QString(PREFIX) + "/lib/mythtv/plugins/lib" + plugname +
                      ".so";
   
    if (m_dict.find(newname) == 0)
    {
        m_dict.insert(newname, new MythPlugin(newname));
        m_dict[newname]->setAutoUnload(true);
    }
   
    int result = m_dict[newname]->init(MYTH_BINARY_VERSION);
   
    if (result == -1)
    {
        m_dict.remove(newname);
        cerr << "Unable to initialize plugin '" << plugname << "'." << endl;
        return false;
    }
   
    return true;
}

void MythPluginManager::run_plugin(const QString & plugname)
{
    QString newname = QString(PREFIX) + "/lib/mythtv/plugins/lib" + plugname +
                      ".so";

    if (m_dict.find(newname) == 0 && init_plugin(plugname) == false)
    {
        cerr << "Unable to run plugin '" << plugname 
             << "': not initialized" << endl;
        return;
    }

    m_dict[newname]->run();
}

void MythPluginManager::config_plugin(const QString & plugname)
{
    QString newname = QString(PREFIX) + "/lib/mythtv/plugins/lib" + plugname +
                      ".so";

    if (m_dict.find(newname) == 0 && init_plugin(plugname) == false)
    {
        cerr << "Unable to run plugin '" << plugname
             << "': not initialized" << endl;
        return;
    }

    m_dict[newname]->config();
}


