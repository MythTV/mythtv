#include "mythplugin.h"
#include "qdict.h"
#include <iostream>

using namespace std;

MythPluginManager::MythPlugin::MythPlugin(const QString &libname)
                 : QLibrary(libname)
{
}

MythPluginManager::MythPlugin::~MythPlugin()
{
}

int MythPluginManager::MythPlugin::init(void)
{
    typedef int (*PluginInitFunc)();
    PluginInitFunc ifunc = (PluginInitFunc)QLibrary::resolve("mythplugin_init");
	
    if (ifunc)
        return ifunc();

    return (-1);
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
   
    int result = m_dict[newname]->init();
   
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


