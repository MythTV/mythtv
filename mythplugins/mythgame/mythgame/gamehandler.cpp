#include "gamehandler.h"
#include "mamehandler.h"
#include "constants.h"

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <iostream>

#include <mythtv/mythcontext.h>

using namespace std;

GameHandler::~GameHandler()
{
}

static QPtrList<GameHandler> *handlers = 0;

static void checkHandlers(MythContext *context)
{
    if (! handlers)
    {
        handlers = new QPtrList<GameHandler>;

        GameHandler::registerHandler(MameHandler::getHandler(context));
    }
}

void GameHandler::processAllGames(MythContext *context)
{
    checkHandlers(context);
    GameHandler *handler = handlers->first();
    while(handler)
    {
        handler->processGames();
        handler = handlers->next();
    }
}

GameHandler* GameHandler::GetHandler(MythContext *context, RomInfo *rominfo)
{
    checkHandlers(context);
    GameHandler *handler = handlers->first();
    while(handler)
    {
        if(rominfo->System() == handler->Systemname())
        {
            return handler;
        }
    }
    return handler;
}

void GameHandler::Launchgame(MythContext *context, RomInfo *romdata)
{
    GameHandler *handler;
    if((handler = GetHandler(context, romdata)))
        handler->start_game(romdata);
}

void GameHandler::EditSettings(MythContext *context, QWidget *parent,
                               RomInfo *romdata)
{
    GameHandler *handler;
    if((handler = GetHandler(context, romdata)))
        handler->edit_settings(parent,romdata);
}

void GameHandler::EditSystemSettings(MythContext *context, QWidget *parent,
                                     RomInfo *romdata)
{
    GameHandler *handler;
    if((handler = GetHandler(context, romdata)))
        handler->edit_system_settings(parent,romdata);
}

RomInfo* GameHandler::CreateRomInfo(MythContext *context, RomInfo* parent)
{
    GameHandler *handler;
    if((handler = GetHandler(context, parent)))
        return handler->create_rominfo(parent);
    return NULL;
}

void GameHandler::registerHandler(GameHandler *handler)
{
    handlers->append(handler);
}
