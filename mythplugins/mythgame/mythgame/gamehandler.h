#ifndef GAMEHANDLER_H_
#define GAMEHANDLER_H_

#include <qstring.h>

#include "rominfo.h"

class GameHandler;

class QSqlDatabase;
class QObject;


class GameHandler
{
  public:
    virtual ~GameHandler();

    static void processAllGames();
    static void registerHandler(GameHandler *);
    static void Launchgame(RomInfo *romdata);
    virtual void start_game(RomInfo * romdata) = 0;
    virtual void processGames() = 0;
    QString Systemname() const { return systemname; }

  protected:
    QString systemname;
};

#endif
