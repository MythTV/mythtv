#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "gamehandler.h"
#include "rominfo.h"
#include "gamesettings.h"
#include "gametree.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythgame", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;


    MythGameSettings settings;
    settings.load(QSqlDatabase::database());
    settings.save(QSqlDatabase::database());

    return 0;
}

int mythplugin_run(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythgame_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    QSqlDatabase *db = QSqlDatabase::database();

    //look for new systems that haven't been added to the database
    //yet and tell them to scan their games

    //build a list of all the systems in the database
    QStringList systems;
    QString thequery = "SELECT DISTINCT system FROM gamemetadata;";
    QSqlQuery query = db->exec(thequery);
    while (query.next())
    {
        QString name = query.value(0).toString();
        systems.append(name);
    }

    //run through the list of registered systems, and if they're not
    //in the database, tell them to scan for games
    for (uint i = 0; i < GameHandler::count(); ++i)
    {
        GameHandler* handler = GameHandler::getHandler(i);
        bool found = systems.find(handler->Systemname()) != systems.end();
        if (!found)
        {
            handler->processGames();
        }
    }

    QString paths = gContext->GetSetting("GameTreeLevels");

    GameTree gametree(gContext->GetMainWindow(), db, "gametree", "game-",
                      paths);
    gametree.exec();

    qApp->removeTranslator(&translator);

    return 0;
}

int mythplugin_config(void)
{
    MythGameSettings settings;
    settings.exec(QSqlDatabase::database());

    return 0;
}

