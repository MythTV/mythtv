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

#define MAX_GAME_TYPES 10

const GameTypes GameTypeList[MAX_GAME_TYPES] =
{   
    { QObject::tr("OTHER"),   "OTHER",  "" },
    { QObject::tr("MAME"),    "MAME",   "" },
    { QObject::tr("NES"),     "NES",    "zip,nes" },
    { QObject::tr("SNES"),    "SNES",   "zip,smc,sfc,fig,swc" },
    { QObject::tr("N64"),     "N64",    "v64,n64" },
    { QObject::tr("PCE/TG16"),"PCE",    "pce" },
    { QObject::tr("GENESIS/MEGADRIVE"), "GENESIS", "smd,bin,md" },
    { QObject::tr("PC GAME"), "PC",     "" },
    { QObject::tr("AMIGA"),   "AMIGA",  "adf" },
    { QObject::tr("ATARI"),   "ATARI",  "bin,a26" },

};

const QString GetGameExtensions(const QString GameType);

class MythGameGeneralSettings;
class MythGamePlayerSettings;
class MythGamePlayerEditor;


class MGSetting: public SimpleDBStorage {
protected:
    MGSetting(const MythGamePlayerSettings& _parent, QString name):
        SimpleDBStorage("gameplayers", name),
        parent(_parent) {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);

    const MythGamePlayerSettings& parent;
};

class MythGameGeneralSettings: virtual public ConfigurationWizard {
public:
    MythGameGeneralSettings();
};

class MythGamePlayerSettings: virtual public ConfigurationWizard {
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
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("gameplayers", "gameplayerid") {
            setName("GamePlayerName");
            setVisible(false);
        };
        virtual QWidget* configWidget(ConfigurationGroup *cg,
                                      QWidget* parent,
                                      const char* widgetName = 0) {
            (void)cg; (void)parent; (void)widgetName;
            return NULL;
        };
    };
    class Name: virtual public MGSetting,
                virtual public LineEditSetting {
    public:
        Name(const MythGamePlayerSettings& parent):
            MGSetting(parent, "playername") {
            setLabel(QObject::tr("Player Name"));
            setHelpText(QObject::tr("Name of this Game and or Emulator"));

        };
    };

private:
    QString settingValue;
    bool changed;
    ID* id;
    Name* name;
protected:

};

class MythGamePlayerEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);

    virtual int exec();
    virtual void load();
    virtual void save() { };

public slots:
    void menu();
    void edit();
    void del();

protected:
};


#endif
