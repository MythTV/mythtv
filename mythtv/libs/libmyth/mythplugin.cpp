#include "mythplugin.h"
#include "qdict.h"
#include <iostream>

using namespace std;

MythPluginManager::MythPlugin::MythPlugin(const QString & libname)
                 :QLibrary(libname)
{
}


MythPluginManager::MythPlugin::~MythPlugin()
{
}


int MythPluginManager::MythPlugin::init(QSqlDatabase *db, MythContext *mc)
{
    typedef int (*PluginInitFunc)(QSqlDatabase *, MythContext *);
	
    PluginInitFunc ifunc = (PluginInitFunc)QLibrary::resolve("plugin_init");
	
    if (ifunc)
        return ifunc(db, mc);

    return (-1);
}


void MythPluginManager::MythPlugin::run ()
{
    typedef int (*PluginRunFunc)();	
    PluginRunFunc rfunc = (PluginRunFunc)QLibrary::resolve("plugin_run");
	
    if (rfunc)
        rfunc();
}

QSqlDatabase * MythPluginManager::_db = NULL;
MythContext * MythPluginManager::_mc = NULL;
QDict<MythPluginManager::MythPlugin> MythPluginManager::_dict;

void MythPluginManager::init (QSqlDatabase * db, MythContext * mc)
{
    _db = db;
    _mc = mc;
    _dict.setAutoDelete(true);
}

bool MythPluginManager::init_plugin (const QString & plugname)
{
    if (NULL == _db || NULL == _mc)
    {
        cerr << "Plugin manager not initialized" << endl;
        return false;
    }
   
    QString newname = QString(PREFIX) + "/lib/mythtv/plugins/lib" + plugname +
                      ".so";
   
    if (0 == _dict.find(newname))
    {
        _dict.insert(newname, new MythPlugin(newname));
        _dict[newname]->setAutoUnload(true);
    }
   
    int result = _dict[newname]->init(_db, _mc);
   
    if (-1 == result)
    {
        _dict.remove(newname);
        cerr << "Unable to initialize plugin '" << plugname << "'." << endl;
        return false;
    }
   
    return true;
}

void MythPluginManager::run_plugin (const QString & plugname)
{
    if (NULL == _db || NULL == _mc)
    {
        cerr << "Plugin manager not initialized" << endl;
        return;
    }
   
    QString newname = QString(PREFIX) + "/lib/mythtv/plugins/lib" + plugname +
                      ".so";

    if (0 == _dict.find(newname) && false == init_plugin(plugname))
    {
        cerr << "Unable to run plugin '" << plugname 
             << "': not initialized" << endl;
        return;
    }
   
    _dict[newname]->run();
}
