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

void startDatabaseTree(MythContext *context, QSqlDatabase *db, QString &paths)
{
    if(context->GetNumSetting("ShotCount")) // this will be a choice in the settings menu.
    {
        ScreenBox screenbox(context, db, paths);
        screenbox.Show();
        screenbox.exec();
    }
    else
    {
        DatabaseBox dbbox(context, db, paths);
        dbbox.Show();

        dbbox.exec();
    }
}

struct GameCBData
{
    MythContext *context;
    QString paths;
    QSqlDatabase *db;
    QValueList<RomInfo> *romlist;
};

void GameCallback(void *data, QString &selection)
{
    GameCBData *gdata = (GameCBData *)data;

    QString sel = selection.lower();

    if (sel == "game_play")
        startDatabaseTree(gdata->context, gdata->db, gdata->paths);
}

void runMenu(MythContext *context, QString themedir, QSqlDatabase *db,
             QString paths, QValueList<RomInfo> &romlist)
{
    ThemedMenu *diag = new ThemedMenu(context, themedir.ascii(), 
                                      "gamemenu.xml");

    GameCBData data;
    data.context = context;
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

    MythContext *context = new MythContext();
    context->LoadQtConfig();

    context->LoadSettingsFiles("mythgame-settings.txt");

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Could not connect to database\n");
        return -1;
    }
    if (!context->OpenDatabase(db))
    {
        printf("could not open db\n");
        return -1;
    }

    //this should only run the first time.
    QString thequery = "SELECT gamename FROM gamemetadata;";
    QSqlQuery query = db->exec(thequery);
    if (!query.isActive() || query.numRowsAffected() <= 0)
    {
        //for each game handler process it's games
        GameHandler::processAllGames(context);
    }
    QString paths = context->GetSetting("TreeLevels");
    QValueList<RomInfo> Romlist;

    QString themename = context->GetSetting("Theme");

    QString themedir = context->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    runMenu(context, themedir, db, paths, Romlist);

    db->close();

    delete context;

    return 0;
}
