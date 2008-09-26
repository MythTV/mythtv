#include <iostream>
using namespace std;

#include <unistd.h>

#include <QDir>
#include <QApplication>

#include "gamehandler.h"
#include "rominfo.h"
#include "gamesettings.h"
#include "gametree.h"
#include "dbcheck.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythversion.h>
#include <mythtv/lcddevice.h>
#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/mythpluginapi.h>
#include <mythtv/libmythui/mythuihelper.h>

#define LOC_ERR QString("MythGame:MAIN Error: ")
#define LOC QString("MythGame:MAIN: ")

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
    QString themedir = GetMythUI()->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(), "game menu");

    GameData data;

    diag->setCallback(GameCallback, &data);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (class LCD * lcd = LCD::Get())
        {
            lcd->switchToTime();
        }
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC_ERR + QString("Couldn't find theme %1")
                                      .arg(themedir));
        delete diag;
    }
}

void runGames(void);

void setupKeys(void)
{
    REG_JUMP("MythGame", "Game frontend", "", runGames);

    REG_KEY("Game", "TOGGLEFAV",     "Toggle the current game as a favorite",
            "?,/");
    REG_KEY("Game", "INCSEARCH",     "Show incremental search dialog",
            "Ctrl+S");
    REG_KEY("Game", "INCSEARCHNEXT", "Incremental search find next match",
            "Ctrl+N");

}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythgame", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;


    gContext->ActivateSettingsCache(false);
    if (!UpgradeGameDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gContext->ActivateSettingsCache(true);

    MythGamePlayerSettings settings;
//    settings.Load();
//    settings.Save();

    setupKeys();

    return 0;
}

void runGames(void)
{
    gContext->addCurrentLocation("mythgame");
    GameTree gametree(gContext->GetMainWindow(), "gametree", "game-");
    gametree.exec();
    gContext->removeCurrentLocation();
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

