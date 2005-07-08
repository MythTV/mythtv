#ifndef GAMEHANDLER_H_
#define GAMEHANDLER_H_

#include <qstring.h>
#include <qwidget.h>

#include "rominfo.h"
#include <mythtv/mythdbcon.h>

class MythMainWindow;
class GameHandler;
class QObject;

class GameHandler
{
  public:
    GameHandler() {  }
    virtual ~GameHandler();

    static void updateSettings(GameHandler*);
    static GameHandler* getHandler(uint i);
    static GameHandler* newHandler(QString name);
    static uint count(void);

    static void GetMetadata(GameHandler *handler, QString GameName, 
                             QString *Genre, int *Year);

    static bool validRom(QString RomName,GameHandler *handler);
    static void buildFileList(QString directory,
                                GameHandler *handler, MSqlQuery *query);

    static void processGames(GameHandler *);
    static void processAllGames(void);
    static void registerHandler(GameHandler *);
    static void Launchgame(RomInfo *romdata);
    static void EditSettings(RomInfo *romdata);
    static void EditSystemSettings(RomInfo *romdata);
    static RomInfo* CreateRomInfo(RomInfo* parent);

//    virtual void start_game(RomInfo *romdata) = 0;
//    virtual void edit_settings(RomInfo *romdata) = 0;
//    virtual void edit_system_settings(RomInfo *romdata) = 0;
    static RomInfo* create_rominfo(RomInfo* parent);
    QString SystemName() const { return systemname; }
    QString SystemCmdLine() const { return commandline; }
    QString SystemRomPath() const { return rompath; }
    QString SystemWorkingPath() const { return workingpath; }
    QString SystemScreenShots() const { return screenshots; }
    uint GamePlayerID() const { return gameplayerid; }
    QString GameType() const { return gametype; }

  protected:
    static GameHandler* GetHandler(RomInfo *rominfo);

    QString systemname;
    QString rompath;
    QString commandline;
    QString workingpath;
    QString screenshots;
    uint gameplayerid;
    QString gametype;
    QStringList validextensions;

  private:
    static GameHandler* newInstance;

};

#endif
