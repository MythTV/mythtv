#include <iostream>
using namespace std;

#include <unistd.h>

#include <QDir>
#include <QApplication>

#include "gameui.h"
#include "gamehandler.h"
#include "rominfo.h"
#include "gamesettings.h"
#include "dbcheck.h"

#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythversion.h>
#include <lcddevice.h>
#include <myththemedmenu.h>
#include <mythpluginapi.h>
#include <mythuihelper.h>

#define LOC_ERR QString("MythGame:MAIN Error: ")
#define LOC QString("MythGame:MAIN: ")

struct GameData
{
};

static void GameCallback(void *data, QString &selection)
{
    GameData *ddata = (GameData *)data;
    QString sel = selection.toLower();

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
        GameHandler *handler = new GameHandler();
        handler->clearAllGameData();
    }

}

static int runMenu(QString which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();

    MythThemedMenu *menu = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(), "game menu");

    GameData data;

    menu->setCallback(GameCallback, &data);
    menu->setKillable();

    if (menu->foundTheme())
    {
        if (LCD *lcd = LCD::Get())
            lcd->switchToTime();

        GetMythMainWindow()->GetMainStack()->AddScreen(menu);
        return 0;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find menu %1 or theme %2")
                              .arg(which_menu).arg(themedir));
        delete menu;
        return -1;
    }
}

static int RunGames(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    GameUI *game = new GameUI(mainStack);

    if (game->Create())
    {
        mainStack->AddScreen(game);
        return 0;
    }
    else
    {
        delete game;
        return -1;
    }
}

static void runGames(void)
{
    RunGames();
}

static void setupKeys(void)
{
    REG_JUMP("MythGame", QT_TRANSLATE_NOOP("MythControls",
        "Game frontend"), "", runGames);

    REG_KEY("Game", "TOGGLEFAV", QT_TRANSLATE_NOOP("MythControls",
        "Toggle the current game as a favorite"), "?,/");
    REG_KEY("Game", "INCSEARCH", QT_TRANSLATE_NOOP("MythControls",
        "Show incremental search dialog"), "Ctrl+S");
    REG_KEY("Game", "INCSEARCHNEXT", QT_TRANSLATE_NOOP("MythControls",
        "Incremental search find next match"), "Ctrl+N");
    REG_KEY("Game","DOWNLOADDATA", QT_TRANSLATE_NOOP("MythControls",
        "Download metadata for current item"), "W");
}

int mythplugin_init(const char *libversion)
{
    if (!gCoreContext->TestPluginVersion("mythgame", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gCoreContext->ActivateSettingsCache(false);
    if (!UpgradeGameDatabaseSchema())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gCoreContext->ActivateSettingsCache(true);

    MythGamePlayerSettings settings;
#if 0
    settings.Load();
    settings.Save();
#endif

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    return RunGames();
}

int mythplugin_config(void)
{
    return runMenu("game_settings.xml");
}

