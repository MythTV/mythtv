#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "gamehandler.h"
#include "databasebox.h"
#include "themedmenu.h"
#include "menubox.h"
#include "settings.h"
#include "rominfo.h"

Settings *globalsettings;

char theprefix[] = "/usr/local";

void startDatabaseTree(QSqlDatabase *db, QString &paths,
                       QValueList<RomInfo> *romlist)
{
    DatabaseBox dbbox(db, paths, romlist);
    dbbox.Show();

    dbbox.exec();
}

bool runMenu(bool usetheme, QString themedir, QSqlDatabase *db,
             QString paths, QValueList<RomInfo> &romlist)
{
    if (usetheme)
    {
        ThemedMenu *diag = new ThemedMenu(themedir.ascii(), "game.menu");
        
        if (diag->foundTheme())
        {
            diag->Show();
            diag->exec();
            
            QString sel = diag->getSelection().lower();

            bool retval = true;

            if (sel == "play_game")
                startDatabaseTree(db, paths, &romlist);
            else
                retval = false;

            delete diag;

            return retval;
        }
    }

    MenuBox diag("MythGame");

    diag.AddButton("Game");

    diag.Show();
       
    int result = diag.exec();

    bool retval = true;
      
    switch (result)
    {   
        case 1: startDatabaseTree(db, paths, &romlist); break;
        default: break;
    }
    if (result == 0)
        retval = false;

    return retval;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    globalsettings = new Settings();

    globalsettings->LoadSettingsFiles("mythgame-settings.txt", theprefix);
    globalsettings->LoadSettingsFiles("theme.txt", theprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", theprefix);
    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Could not connect to database\n");
        return -1;
    }
    db->setDatabaseName(globalsettings->GetSetting("DBName"));
    db->setUserName(globalsettings->GetSetting("DBUserName"));
    db->setPassword(globalsettings->GetSetting("DBPassword"));
    db->setHostName(globalsettings->GetSetting("DBHostName"));
    if (!db->open())
    {
        printf("could not open db\n");
        return -1;
    }

    //for each game handler process it's games
    //TODO: move this to a settings option
    GameHandler::processAllGames();
    QString paths = globalsettings->GetSetting("TreeLevels");
    QValueList<RomInfo> Romlist;

    QString themename = globalsettings->GetSetting("Theme");
    QString themedir = findThemeDir(themename, theprefix);
    bool usetheme = true;
    if (themedir == "")
        usetheme = false;
    while (1)
    {
        if (!runMenu(usetheme, themedir, db, paths, Romlist))
            break;
    }

    db->close();

    delete globalsettings;

    return 0;
}
