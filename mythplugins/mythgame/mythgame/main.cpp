#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "gamehandler.h"
#include "databasebox.h"
#include "screenbox.h"
#include "rominfo.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>

MythContext *gContext;

void startDatabaseTree(QSqlDatabase *db, QString &paths)
{
    if(gContext->GetNumSetting("ShotCount")) // this will be a choice in the settings menu.
    {
        ScreenBox screenbox(db, paths);
        screenbox.Show();
        screenbox.exec();
    }
    else
    {
        DatabaseBox dbbox(db, paths);
        dbbox.Show();

        dbbox.exec();
    }
}

struct GameCBData
{
    QString paths;
    QSqlDatabase *db;
    QValueList<RomInfo> *romlist;
};

void GameCallback(void *data, QString &selection)
{
    GameCBData *gdata = (GameCBData *)data;

    QString sel = selection.lower();

    if (sel == "game_play")
        startDatabaseTree(gdata->db, gdata->paths);
}

void runMenu(QString themedir, QSqlDatabase *db, QString paths, 
             QValueList<RomInfo> &romlist)
{
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), "gamemenu.xml");

    GameCBData data;
    data.paths = paths;
    data.db = db;
    data.romlist = &romlist;

    diag->setCallback(GameCallback, &data);
    diag->setKillable();
        
    if (diag->foundTheme())
    {
        diag->Show();
        diag->exec();
    }     
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }       
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    gContext = new MythContext();

    gContext->LoadSettingsFiles("mythgame-settings.txt");

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Could not connect to database\n");
        return -1;
    }
    if (!gContext->OpenDatabase(db))
    {
        printf("could not open db\n");
        return -1;
    }

    gContext->LoadQtConfig();

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

    QString paths = gContext->GetSetting("TreeLevels");

    QValueList<RomInfo> Romlist;

    QString themename = gContext->GetSetting("Theme");

    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    runMenu(themedir, db, paths, Romlist);

    db->close();

    delete gContext;

    return 0;
}
