#ifndef GAMEHANDLER_H_
#define GAMEHANDLER_H_

#include <qstring.h>
#include <qwidget.h>

#include "rominfo.h"

class MythMainWindow;
class GameHandler;
class QObject;

class GameHandler
{
  public:
    GameHandler() {  }
    virtual ~GameHandler();

    static GameHandler* getHandler(uint i);
    static uint count(void);

    static void processAllGames(void);
    static void registerHandler(GameHandler *);
    static void Launchgame(RomInfo *romdata);
    static void EditSettings(RomInfo *romdata);
    static void EditSystemSettings(RomInfo *romdata);
    static RomInfo* CreateRomInfo(RomInfo* parent);

    virtual void start_game(RomInfo *romdata) = 0;
    virtual void edit_settings(RomInfo *romdata) = 0;
    virtual void edit_system_settings(RomInfo *romdata) = 0;
    virtual void processGames() = 0;
    virtual RomInfo* create_rominfo(RomInfo* parent) = 0;
    QString Systemname() const { return systemname; }

  protected:
    static GameHandler* GetHandler(RomInfo *rominfo);

    QString systemname;
};

#endif
