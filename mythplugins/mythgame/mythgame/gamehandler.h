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

enum GameFound
{
    inNone,
    inFileSystem,
    inDatabase,
    inBoth   
};

class GameScan
{
public:
    GameScan(QString lromname = "", QString lromfullpath = "", 
             int lfoundloc = 0, QString lgamename = "", 
             QString lrompath = "" )
            {
                romname = lromname;
                romfullpath = lromfullpath;
                foundloc = lfoundloc; 
                gamename = lgamename;
                rompath = lrompath;
            }


    QString Rom() const { return romname; }
    QString RomFullPath() const { return romfullpath; }
    int FoundLoc() const { return foundloc; }
    void setLoc( int lfoundloc) { foundloc = lfoundloc; } 
    QString GameName() const { return gamename; }
    QString RomPath() const { return rompath; } 

private:
    QString romname;
    QString romfullpath;
    int foundloc;
    QString gamename;
    QString rompath;
};

typedef QMap <QString, GameScan> GameScanMap;

class GameHandler
{
  public:
    GameHandler()
    {
        m_RemoveAll = false;
        m_KeepAll = false;
        rebuild = false;
    }

    static void updateSettings(GameHandler*);
    static GameHandler* getHandler(uint i);
    static GameHandler* newHandler(QString name);
    static uint count(void);
    void InitMetaDataMap(QString GameType);
    void GetMetadata(GameHandler *handler, QString rom, 
                             QString* Genre, QString* Year, QString* Country,
                             QString* CRC32, QString* GameName,
                             QString* Publisher, QString* Version);

    void promptForRemoval(QString filename, QString RomPath );
    void UpdateGameDB(GameHandler *handler);
    void VerifyGameDB(GameHandler *handler);

    static void clearAllGameData(void); 

    static int buildFileCount(QString directory, GameHandler *handler);
    void buildFileList(QString directory, GameHandler *handler, 
                              MythProgressDialog *pdial, int* filecount);

    void processGames(GameHandler *);
    static void processAllGames(void);
    static void registerHandler(GameHandler *);
    static void Launchgame(RomInfo *romdata, QString systemname);
    static void EditSettings(RomInfo *romdata);
    static void EditSystemSettings(RomInfo *romdata);
    static RomInfo* CreateRomInfo(RomInfo* parent);

    void setRebuild(bool setrebuild) { rebuild = setrebuild; }
    bool needRebuild(void) const { return rebuild; }

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

    bool rebuild;
    int spandisks;
    QString systemname;
    QString rompath;
    QString commandline;
    QString workingpath;
    QString screenshots;
    uint gameplayerid;
    QString gametype;
    QStringList validextensions;

    RomDBMap romDB;
    GameScanMap m_GameMap;

    bool m_RemoveAll;
    bool m_KeepAll;

  private:
    static GameHandler* newInstance;


};

#endif
