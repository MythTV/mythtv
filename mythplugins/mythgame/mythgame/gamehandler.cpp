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

void GameHandler::Launchgame(RomInfo *romdata)
{
    checkHandlers();
    GameHandler *handler = handlers->first();
    while(handler)
    {
        if(romdata->System() == handler->Systemname())
        {
            handler->start_game(romdata);
            return;
        }
        handler = handlers->next();
    }
}

void GameHandler::registerHandler(GameHandler *handler)
{
    handlers->append(handler);
}