// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/lcddevice.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythpluginapi.h>
#include <libmythbase/mythversion.h>
#include <libmythui/myththemedmenu.h>
#include <libmythui/mythuihelper.h>

// MythGame
#include "gamedbcheck.h"
#include "gamehandler.h"
#include "gamesettings.h"
#include "gameui.h"

#define LOC_ERR QString("MythGame:MAIN Error: ")
#define LOC QString("MythGame:MAIN: ")

struct GameData
{
};

static void GameCallback(void *data, QString &selection)
{
    [[maybe_unused]] auto *ddata = static_cast<GameData *>(data);
    QString sel = selection.toLower();

    if (sel == "game_settings")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "gamesettings",
                                              new GameGeneralSettings());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }

    if (sel == "game_players")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "gamesettings",
                                              new GamePlayersList());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "search_for_games")
    {
        GameHandler::processAllGames();
    }
    if (sel == "clear_game_data")
    {
        auto *handler = new GameHandler();
        handler->clearAllGameData();
    }

}

static int runMenu(const QString& which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();

    auto *menu = new MythThemedMenu(themedir, which_menu,
                                    GetMythMainWindow()->GetMainStack(),
                                    "game menu");

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

    LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find menu %1 or theme %2")
        .arg(which_menu, themedir));
    delete menu;
    return -1;
}

static int RunGames(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *game = new GameUI(mainStack);

    if (game->Create())
    {
        mainStack->AddScreen(game);
        return 0;
    }
    delete game;
    return -1;
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
        "Show incremental search dialog"), "Ctrl+S,Search");
    REG_KEY("Game", "INCSEARCHNEXT", QT_TRANSLATE_NOOP("MythControls",
        "Incremental search find next match"), "Ctrl+N");
    REG_KEY("Game","DOWNLOADDATA", QT_TRANSLATE_NOOP("MythControls",
        "Download metadata for current item"), "W");
}

int mythplugin_init(const char *libversion)
{
    if (!MythCoreContext::TestPluginVersion("mythgame", libversion,
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

