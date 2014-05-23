#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

// Qt headers
#include <QString>
#include <QCoreApplication>

// MythTV headers
#include <settings.h>
#include <mythcontext.h>

// The real work.

struct GameTypes {
    QString   nameStr;
    QString   idStr;
    QString   extensions;
};

#define MAX_GAME_TYPES 12

const GameTypes GameTypeList[MAX_GAME_TYPES] =
{
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
};

const QString GetGameTypeName(const QString &GameType);
const QString GetGameTypeExtensions(const QString &GameType);

class MythGameGeneralSettings;
class MythGamePlayerSettings;
class MythGamePlayerEditor;

class GameDBStorage : public SimpleDBStorage
{
  protected:
    GameDBStorage(StorageUser                  *_user,
                  const MythGamePlayerSettings &_parent,
                  const QString                &_name) :
        SimpleDBStorage(_user, "gameplayers", _name), parent(_parent)
    {
    }

    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;

    const MythGamePlayerSettings &parent;
};

class MythGameGeneralSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(MythGameGeneralSettings)

  public:
    MythGameGeneralSettings();
};

class MythGamePlayerSettings : public QObject, public ConfigurationWizard
{
    Q_OBJECT

  public:
    MythGamePlayerSettings();

    int getGamePlayerID(void) const { return id->intValue(); };

    void loadByID(int id);

    static void fillSelections(SelectSetting* setting);
    static QString idToName(int id);

    QString getSourceName(void) const { return name->getValue(); };

    virtual void Save(void)
    {
        if (name)
            ConfigurationWizard::Save();
    }

    virtual void Save(QString /*destination*/) { }

  private:
    class ID : public AutoIncrementDBSetting
    {
      public:
        ID() : AutoIncrementDBSetting("gameplayers", "gameplayerid")
        {
            setName("GamePlayerName");
            setVisible(false);
        }
    };

    class Name : public LineEditSetting, public GameDBStorage
    {
      public:
        Name(const MythGamePlayerSettings &parent) :
            LineEditSetting(this), GameDBStorage(this, parent, "playername")
        {
            setLabel(MythGamePlayerSettings::tr("Player Name"));
            setHelpText(MythGamePlayerSettings::tr("Name of this Game and or "
                                                   "Emulator"));
        }
    };

  private:
    QString settingValue;
    ID     *id;
    Name   *name;
};

class MPUBLIC MythGamePlayerEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    MythGamePlayerEditor();

    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char     *widgetName=0);

    virtual DialogCode exec(bool saveOnExec = true, bool doLoad = true);

    virtual void Load(void);
    virtual void Save(void) { }
    virtual void Save(QString /*destination*/) { }

public slots:
    void menu();
    void edit();
    void del();

  private:
    ListBoxSetting *listbox;
};


#endif
