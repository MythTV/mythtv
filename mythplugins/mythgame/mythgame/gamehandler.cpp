#include "gamehandler.h"
#include "mamehandler.h"
#include "neshandler.h"
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

static void checkHandlers(void)
{
    if (! handlers)
    {
        handlers = new QPtrList<GameHandler>;

        if (gContext->GetSetting("XMameBinary") != "")
            GameHandler::registerHandler(MameHandler::getHandler());
        if (gContext->GetSetting("NesBinary") != "")
            GameHandler::registerHandler(NesHandler::getHandler());
    }
}

GameHandler* GameHandler::getHandler(uint i)
{
    checkHandlers();
    return handlers->at(i);
}

uint GameHandler::count(void)
{
    checkHandlers();
    return handlers->count();
}

void GameHandler::processAllGames(void)
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
        handler = handlers->next();
    }
    return handler;
}

void GameHandler::Launchgame(RomInfo *romdata)
{
    GameHandler *handler;
    if((handler = GetHandler(romdata)))
        handler->start_game(romdata);
}

void GameHandler::EditSettings(QWidget *parent, RomInfo *romdata)
{
    GameHandler *handler;
    if((handler = GetHandler(romdata)))
        handler->edit_settings(parent,romdata);
}

void GameHandler::EditSystemSettings(QWidget *parent, RomInfo *romdata)
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
