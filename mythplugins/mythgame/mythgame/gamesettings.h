#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

// The real work.

struct GameTypes {
    QString   nameStr;
    QString   idStr;
    QString   extensions;
};

#define MAX_GAME_TYPES 12

// TODO FIXME Can't initialize translated values statically. They are only
//            translated if you get lucky with dynamic linking order.
const GameTypes GameTypeList[MAX_GAME_TYPES] =
{   
    { QObject::tr("OTHER"),   "OTHER",  "" },
    { QObject::tr("AMIGA"),   "AMIGA",  "adf,ipf" },
    { QObject::tr("ATARI"),   "ATARI",  "bin,a26" },
    { QObject::tr("GAMEGEAR"),    "GAMEGEAR",   "gg" },
    { QObject::tr("GENESIS/MEGADRIVE"), "GENESIS", "smd,bin,md" },
    { QObject::tr("MAME"),    "MAME",   "" },
    { QObject::tr("N64"),     "N64",    "v64,n64" },
    { QObject::tr("NES"),     "NES",    "zip,nes" },
    { QObject::tr("PC GAME"), "PC",     "" },
    { QObject::tr("PCE/TG16"),"PCE",    "pce" },
    { QObject::tr("SEGA/MASTER SYSYTEM"), "SEGA", "sms" },
    { QObject::tr("SNES"),    "SNES",   "zip,smc,sfc,fig,swc" }
};

const QString GetGameExtensions(const QString GameType);

class MythGameGeneralSettings;
class MythGamePlayerSettings;
class MythGamePlayerEditor;

class GameDBStorage : public SimpleDBStorage
{
  protected:
    GameDBStorage(Setting                      *_setting,
                  const MythGamePlayerSettings &_parent,
                  const QString                &_name) :
        SimpleDBStorage(_setting, "gameplayers", _name), parent(_parent)
    {
        _setting->setName(_name);
    }

    virtual QString setClause(MSqlBindings &bindings);
    virtual QString whereClause(MSqlBindings &bindings);

    const MythGamePlayerSettings& parent;
};

class MythGameGeneralSettings : public ConfigurationWizard
{
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

    virtual void save() {
        if (name)
            ConfigurationWizard::save();
    };

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
            setLabel(QObject::tr("Player Name"));
            setHelpText(QObject::tr("Name of this Game and or Emulator"));
        }
    };

  private:
    QString settingValue;
    bool    changed;
    ID     *id;
    Name   *name;
};

class MythGamePlayerEditor :
    public QObject, public ConfigurationDialog, public Storage
{
    Q_OBJECT

  public:
    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char     *widgetName);

    virtual int exec();
    virtual void load();
    virtual void save() { };

public slots:
    void menu();
    void edit();
    void del();

  private:
    ListBoxSetting *listbox;
};


#endif
