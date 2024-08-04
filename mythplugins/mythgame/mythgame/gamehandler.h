// -*- Mode: c++ -*-
#ifndef GAMEHANDLER_H_
#define GAMEHANDLER_H_

// C++
#include <utility>

// Qt
#include <QEvent>
#include <QMap>
#include <QObject>
#include <QStringList>

// MythTV
#include <libmythbase/mythdbcon.h>

// MythGame
#include "rom_metadata.h"
#include "rominfo.h"

class MythMainWindow;
class GameHandler;

enum GameFound : std::uint8_t
{
    inNone,
    inFileSystem,
    inDatabase,
    inBoth
};

class GameScan
{
  public:
    explicit GameScan(QString lromname = "", QString lromfullpath = "",
             int lfoundloc    = 0,  QString lgamename = "",
             QString lrompath = "") :
         m_romname(std::move(lromname)),
         m_romfullpath(std::move(lromfullpath)),
         m_gamename(std::move(lgamename)),
         m_rompath(std::move(lrompath)),
         m_foundloc(lfoundloc) {}

    QString Rom(void)         const { return m_romname;       }
    QString RomFullPath(void) const { return m_romfullpath;   }
    int     FoundLoc(void)    const { return m_foundloc;      }
    void    setLoc(int lfoundloc)   { m_foundloc = lfoundloc; }
    QString GameName(void)    const { return m_gamename;      }
    QString RomPath(void)     const { return m_rompath;       }

  private:
    QString m_romname;
    QString m_romfullpath;
    QString m_gamename;
    QString m_rompath;
    int     m_foundloc;
};

Q_DECLARE_METATYPE(GameScan)

using GameScanMap = QMap<QString, GameScan>;

class MythUIProgressDialog;
class GameHandler : public QObject
{
    Q_OBJECT

  public:
    GameHandler() = default;

    static void updateSettings(GameHandler *handler);
    static GameHandler *getHandler(uint i);
    static GameHandler *newHandler(QString name);
    static uint count(void);
    void InitMetaDataMap(const QString& GameType);
    void GetMetadata(GameHandler *handler, const QString& rom,
                             QString* Genre, QString* Year, QString* Country,
                             QString* CRC32, QString* GameName,
                             QString* Plot, QString* Publisher, QString* Version,
                             QString* Fanart, QString* Boxart);

    void promptForRemoval(const GameScan& scan);
    void UpdateGameDB(GameHandler *handler);
    void VerifyGameDB(GameHandler *handler);

    void clearAllGameData(void);

    static int buildFileCount(const QString& directory, GameHandler *handler);
    void buildFileList(const QString& directory, GameHandler *handler,
                       int* filecount);

    void processGames(GameHandler *handler);
    static void processAllGames(void);
    static void registerHandler(GameHandler *handler);
    static void Launchgame(RomInfo *romdata, const QString& systemname);
    static void EditSettings(RomInfo *romdata);
    static void EditSystemSettings(RomInfo *romdata);
    static RomInfo* CreateRomInfo(RomInfo* parent);

    void setRebuild(bool setrebuild) { m_rebuild = setrebuild; }
    bool needRebuild(void) const { return m_rebuild; }

    static RomInfo* create_rominfo(RomInfo* parent);
    bool SpanDisks() const { return m_spandisks; }
    QString SystemName() const { return m_systemname; }
    QString SystemCmdLine() const { return m_commandline; }
    QString SystemRomPath() const { return m_rompath; }
    QString SystemWorkingPath() const { return m_workingpath; }
    QString SystemScreenShots() const { return m_screenshots; }
    uint GamePlayerID() const { return m_gameplayerid; }
    QString GameType() const { return m_gametype; }
    QStringList ValidExtensions() const { return m_validextensions; }

    static void clearAllMetadata(void);

    static GameHandler* GetHandler(RomInfo *rominfo);
    static GameHandler* GetHandlerByName(const QString& systemname);

  protected:
    void customEvent(QEvent *event) override; // QObject

    bool    m_rebuild      {false};
    bool    m_spandisks    {false};
    QString m_systemname;
    QString m_rompath;
    QString m_commandline;
    QString m_workingpath;
    QString m_screenshots;
    uint    m_gameplayerid {0};
    QString m_gametype;
    QStringList m_validextensions;

    RomDBMap    m_romDB;
    GameScanMap m_gameMap;

    bool m_removeAll       {false};
    bool m_keepAll         {false};

  private:
    void CreateProgress(const QString& message);
    static GameHandler *s_newInstance;

    MythUIProgressDialog *m_progressDlg {nullptr};
};

#endif
