#ifndef GAMEHANDLER_H_
#define GAMEHANDLER_H_

#include <qstring.h>
#include <qwidget.h>

#include "rominfo.h"

class GameHandler;
class QSqlDatabase;
class QObject;
class MythContext;

class GameHandler
{
  public:
    GameHandler(MythContext *context) { m_context = context; }
    virtual ~GameHandler();

    static void processAllGames(MythContext *context);
    static void registerHandler(GameHandler *);
    static void Launchgame(MythContext *context, RomInfo *romdata);
    static void EditSettings(MythContext *contet, QWidget* parent,
                             RomInfo *romdata);
    static void EditSystemSettings(MythContext *context, QWidget* parent,
                                   RomInfo *romdata);
    static RomInfo* CreateRomInfo(MythContext *context, RomInfo* parent);
    virtual void start_game(RomInfo *romdata) = 0;
    virtual void edit_settings(QWidget *parent,RomInfo *romdata) = 0;
    virtual void edit_system_settings(QWidget *parent,RomInfo *romdata) = 0;
    virtual void processGames() = 0;
    virtual RomInfo* create_rominfo(RomInfo* parent) = 0;
    QString Systemname() const { return systemname; }

  protected:
    static GameHandler* GetHandler(MythContext *context, RomInfo *rominfo);

    QString systemname;
    MythContext *m_context;
};

#endif
