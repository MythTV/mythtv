#include "gamehandler.h"
#include "mamehandler.h"
#include "constants.h"

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <iostream.h>

GameHandler::~GameHandler()
{
}

static QPtrList<GameHandler> *handlers = 0;

static void checkHandlers()
{
    if (! handlers)
    {
        handlers = new QPtrList<GameHandler>;

        GameHandler::registerHandler(MameHandler::getHandler());
    }
}

void GameHandler::processAllGames()
{
    checkHandlers();
    GameHandler *handler = handlers->first();
    while(handler)
    {
        handler->processGames();
        handler = handlers->next();
    }
}

GameHandler* GameHandler::GetHandler(RomInfo *rominfo)
{
    checkHandlers();
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

void GameHandler::Launchgame(RomInfo *romdata)
{
    GameHandler *handler;
    if((handler = GetHandler(romdata)))
        handler->start_game(romdata);
}

void GameHandler::EditSettings(QWidget *parent,RomInfo *romdata)
{
    GameHandler *handler;
    if((handler = GetHandler(romdata)))
        handler->edit_settings(parent,romdata);
}

void GameHandler::EditSystemSettings(QWidget *parent,RomInfo *romdata)
{
    GameHandler *handler;
    if((handler = GetHandler(romdata)))
        handler->edit_system_settings(parent,romdata);
}

RomInfo* GameHandler::CreateRomInfo(RomInfo* parent)
{
    GameHandler *handler;
    if((handler = GetHandler(parent)))
        return handler->create_rominfo(parent);
    return NULL;
}

void GameHandler::registerHandler(GameHandler *handler)
{
    handlers->append(handler);
}
