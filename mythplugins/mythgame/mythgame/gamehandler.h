#ifndef GAMEHANDLER_H_
#define GAMEHANDLER_H_

#include <qstring.h>
#include <qwidget.h>

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
    static void EditSettings(QWidget* parent,RomInfo *romdata);
    static RomInfo* CreateRomInfo(RomInfo* parent);
    virtual void start_game(RomInfo *romdata) = 0;
    virtual void edit_settings(QWidget *parent,RomInfo *romdata) = 0;
    virtual void processGames() = 0;
    virtual RomInfo* create_rominfo(RomInfo* parent) = 0;
    QString Systemname() const { return systemname; }

  protected:
    static GameHandler* GetHandler(RomInfo *rominfo);

    QString systemname;
};

#endif
