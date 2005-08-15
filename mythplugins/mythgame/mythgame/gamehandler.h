#ifndef GAMEHANDLER_H_
#define GAMEHANDLER_H_

#include <qstring.h>
#include <qwidget.h>

#include "rom_metadata.h"
#include "rominfo.h"
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>

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

    static void GetMetadata(GameHandler *handler, QString rom, 
                             QString* Genre, int* Year, QString* Country,
                             QString* CRC32);

    static int buildFileCount(QString directory, GameHandler *handler);
    void buildFileList(QString directory, GameHandler *handler, 
                              MSqlQuery *query, MythProgressDialog *pdial, 
                              int indepth, int* filecount);

    void processGames(GameHandler *);
    static void processAllGames(void);
    static void registerHandler(GameHandler *);
    static void Launchgame(RomInfo *romdata, QString systemname);
    static void EditSettings(RomInfo *romdata);
    static void EditSystemSettings(RomInfo *romdata);
    static RomInfo* CreateRomInfo(RomInfo* parent);

    static RomInfo* create_rominfo(RomInfo* parent);
    int SpanDisks() const { return spandisks; }
    QString SystemName() const { return systemname; }
    QString SystemCmdLine() const { return commandline; }
    QString SystemRomPath() const { return rompath; }
    QString SystemWorkingPath() const { return workingpath; }
    QString SystemScreenShots() const { return screenshots; }
    uint GamePlayerID() const { return gameplayerid; }
    QString GameType() const { return gametype; }

  protected:
    static GameHandler* GetHandler(RomInfo *rominfo);
    static GameHandler* GameHandler::GetHandlerByName(QString systemname);

    int spandisks;
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
