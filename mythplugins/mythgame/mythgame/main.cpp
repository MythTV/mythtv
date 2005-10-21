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
    REG_KEY("Game", "JUMPA", "Jump to the letter A", "Ctrl+A");
    REG_KEY("Game", "JUMPB", "Jump to the letter B", "Ctrl+B");
    REG_KEY("Game", "JUMPC", "Jump to the letter C", "Ctrl+C");
    REG_KEY("Game", "JUMPD", "Jump to the letter D", "Ctrl+D");
    REG_KEY("Game", "JUMPE", "Jump to the letter E", "Ctrl+E");
    REG_KEY("Game", "JUMPF", "Jump to the letter F", "Ctrl+F");
    REG_KEY("Game", "JUMPG", "Jump to the letter G", "Ctrl+G");
    REG_KEY("Game", "JUMPH", "Jump to the letter H", "Ctrl+H");
    REG_KEY("Game", "JUMPI", "Jump to the letter I", "Ctrl+I");
    REG_KEY("Game", "JUMPJ", "Jump to the letter J", "Ctrl+J");
    REG_KEY("Game", "JUMPK", "Jump to the letter K", "Ctrl+K");
    REG_KEY("Game", "JUMPL", "Jump to the letter L", "Ctrl+L");
    REG_KEY("Game", "JUMPM", "Jump to the letter M", "Ctrl+M");
    REG_KEY("Game", "JUMPN", "Jump to the letter N", "Ctrl+N");
    REG_KEY("Game", "JUMPO", "Jump to the letter O", "Ctrl+O");
    REG_KEY("Game", "JUMPP", "Jump to the letter P", "Ctrl+P");
    REG_KEY("Game", "JUMPQ", "Jump to the letter Q", "Ctrl+Q");
    REG_KEY("Game", "JUMPR", "Jump to the letter R", "Ctrl+R");
    REG_KEY("Game", "JUMPS", "Jump to the letter S", "Ctrl+S");
    REG_KEY("Game", "JUMPT", "Jump to the letter T", "Ctrl+T");
    REG_KEY("Game", "JUMPU", "Jump to the letter U", "Ctrl+U");
    REG_KEY("Game", "JUMPV", "Jump to the letter V", "Ctrl+V");
    REG_KEY("Game", "JUMPW", "Jump to the letter W", "Ctrl+W");
    REG_KEY("Game", "JUMPX", "Jump to the letter X", "Ctrl+X");
    REG_KEY("Game", "JUMPY", "Jump to the letter Y", "Ctrl+Y");
    REG_KEY("Game", "JUMPZ", "Jump to the letter Z", "Ctrl+Z");
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

