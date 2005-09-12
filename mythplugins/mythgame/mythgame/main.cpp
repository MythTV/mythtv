#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <unistd.h>

#include "gamehandler.h"
#include "rominfo.h"
#include "gamesettings.h"
#include "gametree.h"
#include "dbcheck.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/lcddevice.h>

struct GameData
{
};

void GameCallback(void *data, QString &selection)
{
    GameData *ddata = (GameData *)data;
    QString sel = selection.lower();

    (void)ddata;

    if (sel == "game_settings")
    {   
        MythGameGeneralSettings settings;
        settings.exec();
    }

    if (sel == "game_players")
    {
	MythGamePlayerEditor mgpe;
	mgpe.exec();
    }
    else if (sel == "search_for_games")
    {
        GameHandler::processAllGames();
    }
    if (sel == "clear_game_data")
    {
        GameHandler::clearAllGameData();
    }

}

void runMenu(QString which_menu)
{
    QString themedir = gContext->GetThemeDir();

    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), which_menu,
                                      gContext->GetMainWindow(), "game menu");

    GameData data;

    diag->setCallback(GameCallback, &data);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (class LCD * lcd = LCD::Get())
        {
            lcd->switchToTime();
        }
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runGames(void);

void setupKeys(void)
{
    REG_JUMP("MythGame", "Game frontend", "", runGames);

    REG_KEY("Game", "TOGGLEFAV", "Toggle the current game as a favorite", 
            "?,/");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythgame", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;


    if (!UpgradeGameDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }

    MythGamePlayerSettings settings;
//    settings.load();
//    settings.save();

    setupKeys();

    return 0;
}

void runGames(void)
{
    GameTree gametree(gContext->GetMainWindow(), "gametree", "game-");
    gametree.exec();
}

int mythplugin_run(void)
{
    runGames();
    return 0;
}

int mythplugin_config(void)
{
    runMenu("game_settings.xml");
    return 0;
}

