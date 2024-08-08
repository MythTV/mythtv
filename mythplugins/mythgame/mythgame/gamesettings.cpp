// C++
#include <array>

// Qt
#include <QCoreApplication>

// MythTV
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdirs.h>

// MythGame
#include "gamesettings.h"

struct GameTypes {
    QString   m_nameStr;
    QString   m_idStr;
    QString   m_extensions;
};

const std::array<GameTypes,12> GameTypeList
{{
    { QT_TRANSLATE_NOOP("(GameTypes)", "OTHER"),   "OTHER",  "" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "AMIGA"),   "AMIGA",  "adf,ipf" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "ATARI"),   "ATARI",  "bin,a26" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "GAMEGEAR"),    "GAMEGEAR",   "gg" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "GENESIS/MEGADRIVE"), "GENESIS", "smd,bin,md" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "MAME"),    "MAME",   "" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "N64"),     "N64",    "v64,n64" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "NES"),     "NES",    "zip,nes" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "PC GAME"), "PC",     "" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "PCE/TG16"),"PCE",    "pce" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "SEGA/MASTER SYSTEM"), "SEGA", "sms" },
    { QT_TRANSLATE_NOOP("(GameTypes)", "SNES"),    "SNES",   "zip,smc,sfc,fig,swc" }
}};

QString GetGameTypeName(const QString &GameType)
{
    auto sametype = [GameType](const auto & console)
        { return console.m_idStr == GameType; };
    const auto *const con = std::find_if(GameTypeList.cbegin(), GameTypeList.cend(), sametype);
    return (con != GameTypeList.cend())
        ? QCoreApplication::translate("(GameTypes)", con->m_nameStr.toUtf8())
        : "";
}

QString GetGameTypeExtensions(const QString &GameType)
{
    auto sametype = [GameType](const auto & console)
        { return console.m_idStr == GameType; };
    const auto *const con = std::find_if(GameTypeList.cbegin(), GameTypeList.cend(), sametype);
    return (con != GameTypeList.cend()) ? con->m_extensions : "";
}

// -----------------------------------------------------------------------

// General Settings
#undef TR
#define TR GameGeneralSettings::tr

static HostTextEditSetting *GameAllTreeLevels()
{
    auto *gc = new HostTextEditSetting("GameAllTreeLevels");
    gc->setLabel(TR("Game display order"));
    gc->setValue("system gamename");
    gc->setHelpText(TR("Order in which to sort the "
                       "games - this is for all "
                       "systems. Available choices: "
                       "system, year, genre and "
                       "gamename"));
    return gc;
}

static HostTextEditSetting *GameFavTreeLevels()
{
    auto *gc = new HostTextEditSetting("GameFavTreeLevels");
    gc->setLabel(TR("Favorite display order"));
    gc->setValue("gamename");
    gc->setHelpText(TR("Order in which to sort the "
                       "games marked as favorites "
                       "- this is for all systems. "
                       "Available choices: system, "
                       "year, genre and gamename"));
    return gc;
}

static HostCheckBoxSetting *GameDeepScan()
{
    auto *gc = new HostCheckBoxSetting("GameDeepScan");
    gc->setLabel(TR("Indepth Game Scan"));
    gc->setHelpText(
                TR("Enabling this causes a game scan to "
                   "gather CRC values and attempt to find out "
                   "more detailed information about the game: "
                   "NOTE this can greatly increase the time a "
                   "game scan takes based on the amount of "
                   "games scanned."));
    return gc;
}

static HostCheckBoxSetting *GameRemovalPrompt()
{
    auto *gc = new HostCheckBoxSetting("GameRemovalPrompt");
    gc->setLabel(TR("Prompt for removal of deleted ROM(s)"));
    gc->setHelpText(TR("This enables a prompt for "
                       "removing deleted ROMs from "
                       "the database during a game "
                       "scan"));

    return gc;
}

static HostCheckBoxSetting *GameShowFileNames()
{
    auto *gc = new HostCheckBoxSetting("GameShowFileNames");
    gc->setLabel(TR("Display Files Names in Game "
                    "Tree"));
    gc->setHelpText(TR("Enabling this causes the "
                       "filenames to be displayed in "
                       "the game tree rather than the "
                       "trimmed/looked up game name"));
    return gc;
}

static HostCheckBoxSetting *GameTreeView()
{
    auto *gc = new HostCheckBoxSetting("GameTreeView");
    gc->setLabel(TR("Hash filenames in display"));
    gc->setValue(0);
    gc->setHelpText(TR("Enable hashing of names in "
                       "the display tree. This can "
                       "make navigating long lists "
                       "a little faster"));
    return gc;
}

static HostTextEditSetting *GetScreenshotDir()
{
    auto *gc = new HostTextEditSetting("mythgame.screenshotdir");
    gc->setLabel(TR("Directory where Game Screenshots "
                    "are stored"));
    gc->setValue(GetConfDir() + "/MythGame/Screenshots");
    gc->setHelpText(TR("This directory will be the "
                       "default browse location when "
                       "assigning screenshots."));
    return gc;
}

static HostTextEditSetting *GetFanartDir()
{
    auto *gc = new HostTextEditSetting("mythgame.fanartdir");
    gc->setLabel(TR("Directory where Game Fanart is "
                    "stored"));
    gc->setValue(GetConfDir() + "/MythGame/Fanart");
    gc->setHelpText(TR("This directory will be the "
                       "default browse location when "
                       "assigning fanart."));
    return gc;
}

static HostTextEditSetting *GetBoxartDir()
{
    auto *gc = new HostTextEditSetting("mythgame.boxartdir");
    gc->setLabel(TR("Directory where Game Boxart is "
                    "stored"));
    gc->setValue(GetConfDir() + "/MythGame/Boxart");
    gc->setHelpText(TR("This directory will be the "
                       "default browse location when "
                       "assigning boxart."));
    return gc;
}

GameGeneralSettings::GameGeneralSettings()
{
    setLabel(tr("MythGame Settings -- General"));
    addChild(GameAllTreeLevels());
    addChild(GameFavTreeLevels());
    addChild(GameDeepScan());
    addChild(GameRemovalPrompt());
    addChild(GameShowFileNames());
    addChild(GameTreeView());
    addChild(GetScreenshotDir());
    addChild(GetFanartDir());
    addChild(GetBoxartDir());
}

// -----------------------------------------------------------------------

// Player Settings
#undef TR
#define TR GamePlayerSetting::tr

/// Game player database table reader/writer
struct GameDBStorage : public SimpleDBStorage
{
    GameDBStorage(StorageUser *user, const PlayerId &id, const QString &name)
        : SimpleDBStorage(user, "gameplayers", name), m_id(id) {}
protected:
    const PlayerId &m_id;

    QString GetWhereClause(MSqlBindings &bindings) const override // SimpleStorage
    {
        bindings.insert(":PLAYERID", m_id.Value());
        return "gameplayerid = :PLAYERID";
    }

    QString GetSetClause(MSqlBindings &bindings) const override // SimpleStorage
    {
        bindings.insert(":SETPLAYERID", m_id.Value());
        bindings.insert(":SETCOLUMN",   m_user->GetDBValue());
        return QString("gameplayerid = :SETPLAYERID, "
                       "%2 = :SETCOLUMN").arg(GetColumnName());
    }
};

/// Base for Game textual settings
struct TextEdit : public MythUITextEditSetting
{
    explicit TextEdit(const PlayerId &parent, const QString& column) :
        MythUITextEditSetting(new GameDBStorage(this, parent, column))
    {}
};

struct Name : public TextEdit
{
    explicit Name(const PlayerId &parent)
        : TextEdit(parent, "playername")
    {
        setLabel(TR("Player Name"));
        setHelpText(TR("Name of this Game and or Emulator"));
    }
};

struct Command : public TextEdit
{
    explicit Command(const PlayerId &parent)
        : TextEdit(parent, "commandline")
    {
        setLabel(TR("Command"));
        setHelpText(TR("Binary and optional parameters. Multiple commands "
                       "separated with \';\' . Use \%s for the ROM name. "
                       "\%d1, \%d2, \%d3 and \%d4 represent disks in a "
                       "multidisk/game. %s auto appended if not specified"));
    }
};

struct GameType : public MythUIComboBoxSetting
{
    explicit GameType(const PlayerId &parent) :
        MythUIComboBoxSetting(new GameDBStorage(this, parent, "gametype"))
    {
        //: Game type
        setLabel(TR("Type"));
        for (const auto & console : GameTypeList)
        {
            addSelection(QCoreApplication::translate("(GameTypes)",
                                                     console.m_nameStr.toUtf8()),
                         console.m_idStr);
        }
        setValue(0);
        setHelpText(TR("Type of Game/Emulator. Mostly for informational "
                       "purposes and has little effect on the "
                       "function of your system."));
    }
};

struct RomPath : public TextEdit
{
    explicit RomPath(const PlayerId &parent)
        : TextEdit(parent, "rompath")
    {
        setLabel(TR("ROM Path"));
        setHelpText(TR("Location of the ROM files for this emulator"));
    }
};

struct WorkingDirPath : public TextEdit
{
    explicit WorkingDirPath(const PlayerId &parent)
        : TextEdit(parent, "workingpath")
    {
        setLabel(TR("Working Directory"));
        setHelpText(TR("Directory to change to before launching emulator. "
                       "Blank is usually fine"));
    }
};

struct Extensions : public TextEdit
{
    explicit Extensions(const PlayerId &parent)
        : TextEdit(parent, "extensions")
    {
        setLabel(TR("File Extensions"));
        setHelpText(TR("A comma separated list of all file extensions for this "
                       "emulator. Blank means any file under ROM PATH is "
                       "considered to be used with this emulator"));
    }
};

struct AllowMultipleRoms : public MythUICheckBoxSetting
{
    explicit AllowMultipleRoms(const PlayerId &parent) :
        MythUICheckBoxSetting(new GameDBStorage(this, parent, "spandisks"))
    {
        setLabel(TR("Allow games to span multiple ROMs/disks"));
        setHelpText(TR("This setting means that we will look for items like "
                       "game.1.rom, game.2.rom and consider them a single game."));
    }
};

/// Settings for a game player
GamePlayerSetting::GamePlayerSetting(const QString& name, uint id)
    : m_id(id)
{
    setName(name);

    // Pre-set name for new players
    auto *nameChild = new Name(m_id);
    nameChild->setValue(name);

    addChild(nameChild);
    addChild(new GameType(m_id));
    addChild(new Command(m_id));
    addChild(new RomPath(m_id));
    addChild(new WorkingDirPath(m_id));
    addChild(new Extensions(m_id));
    addChild(new AllowMultipleRoms(m_id));
}

void GamePlayerSetting::Save()
{
    // Allocate id for new player
    m_id.Save();
    GroupSetting::Save();
}

void GamePlayerSetting::deleteEntry()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM gameplayers "
                  "WHERE gameplayerid = :PLAYERID");

    query.bindValue(":PLAYERID", m_id.Value());

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Deleting MythGamePlayerSettings:", query);
}

// -----------------------------------------------------------------------

GamePlayersList::GamePlayersList()
{
    setLabel(tr("Game Players"));
}

void GamePlayersList::Load()
{
    clearSettings();

    auto *newPlayer = new ButtonStandardSetting(tr("(New Game Player)"));
    addChild(newPlayer);
    connect(newPlayer, &ButtonStandardSetting::clicked,
            this,      &GamePlayersList::NewPlayerDialog);

    //: %1 is the player/emulator name, %2 is the type of player/emulator
    QString playerDisp = tr("%1 (%2)", "Game player/emulator display");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT gameplayerid, playername, gametype "
                  "FROM gameplayers "
                  "WHERE playername <> '' "
                  "ORDER BY playername;");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("GamePlayersSetting::Load", query);
    }
    else
    {
        while (query.next())
        {
            int     id   = query.value(0).toInt();
            QString name = query.value(1).toString();
            QString type = query.value(2).toString();

            auto *child = new GamePlayerSetting(name, id);
            addChild(child);
            child->setLabel(playerDisp.arg(name, GetGameTypeName(type)));
        }
    }

    GroupSetting::Load();
}

void GamePlayersList::NewPlayerDialog() const
{
    MythScreenStack *stack = GetMythMainWindow()->GetStack("popup stack");
    auto *nameDialog = new MythTextInputDialog(stack, tr("Player Name"));

    if (nameDialog->Create())
    {
        stack->AddScreen(nameDialog);
        connect(nameDialog, &MythTextInputDialog::haveResult,
                this,       &GamePlayersList::CreateNewPlayer);
    }
    else
    {
        delete nameDialog;
    }
}

void GamePlayersList::CreateNewPlayer(const QString& name)
{
    if (name.isEmpty())
        return;

    // Database name must be unique
    for (StandardSetting* child : std::as_const(*getSubSettings()))
    {
        if (child->getName() == name)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Player name %1 is already used").arg(name));
            return;
        }
    }

    addChild(new GamePlayerSetting(name));

    // Redraw list
    setVisible(true);
}
