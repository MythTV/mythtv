#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "gamesettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>


const QString GetGameExtensions(const QString GameType)
{
    QString result = "";

    for (int i = 0; i < MAX_GAME_TYPES; i++)
    {
        if (GameTypeList[i].idStr == GameType) {
            result = GameTypeList[i].extensions;
            break;
        }
    }

    return result;
}

// General Settings

static HostLineEdit *GameAllTreeLevels()
{
    HostLineEdit *gc = new HostLineEdit("GameAllTreeLevels");
    gc->setLabel(QObject::tr("Game display order"));
    gc->setValue("system year genre gamename");
    gc->setHelpText(QObject::tr("Order in which to sort the games "
                    "- this is for all systems. Available choices: "
                    "system, year, genre and gamename"));
    return gc;
};

static HostLineEdit *GameFavTreeLevels()
{
    HostLineEdit *gc = new HostLineEdit("GameFavTreeLevels");
    gc->setLabel(QObject::tr("Favourite display order"));
    gc->setValue("gamename");
    gc->setHelpText(QObject::tr("Order in which to sort the games "
                    "marked as favourites "
                    "- this is for all systems. Available choices: "
                    "system, year, genre and gamename"));
    return gc;
};

QString MGSetting::whereClause(void) {
    return QString("gameplayerid = %1").arg(parent.getGamePlayerID());
}

QString MGSetting::setClause(void) {
    return QString("gameplayerid = %1, %2 = '%3'")
        .arg(parent.getGamePlayerID())
        .arg(getColumn())
        .arg(getValue());
}

MythGameGeneralSettings::MythGameGeneralSettings()
{
    VerticalConfigurationGroup *general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("MythGame Settings -- General"));
    general->addChild(GameAllTreeLevels());
    general->addChild(GameFavTreeLevels());
    addChild(general);

}

class Command: virtual public MGSetting, virtual public LineEditSetting {
public:
    Command(const MythGamePlayerSettings& parent):
        MGSetting(parent, "commandline") {
        setLabel(QObject::tr("Command"));
        setHelpText(QObject::tr("Location of the Emulator binary and optional parameters "));

    };
};

class GameType: public ComboBoxSetting, public MGSetting {
public:
    GameType(const MythGamePlayerSettings& parent):
        MGSetting(parent, "gametype") {
        setLabel(QObject::tr("Type"));
        for (int i = 0; i < MAX_GAME_TYPES; i++)
        {
            addSelection(GameTypeList[i].nameStr, GameTypeList[i].idStr);
        }
        setHelpText(QObject::tr("Type of Game/Emulator. Mostly for informational purposes and has little effect on the function of your system."));
    };
};

class RomPath: virtual public MGSetting, virtual public LineEditSetting {
public:
    RomPath(const MythGamePlayerSettings& parent):
        MGSetting(parent, "rompath") {
        setLabel(QObject::tr("Rom Path"));
        setHelpText(QObject::tr("Location of the ROM files for this emulator"));
    };
};

class WorkingDirPath: virtual public MGSetting, virtual public LineEditSetting {
public:
    WorkingDirPath(const MythGamePlayerSettings& parent):
        MGSetting(parent, "workingpath") {
        setLabel(QObject::tr("Working Directory"));
        setHelpText(QObject::tr("Directory to change to before launching emulator. Blank is usually fine"));
    };
};

class Extensions: virtual public MGSetting, virtual public LineEditSetting {
public:
    Extensions(const MythGamePlayerSettings& parent):
        MGSetting(parent, "extensions") {
        setLabel(QObject::tr("File Extensions"));
        setHelpText(QObject::tr("List of all file extensions to be used for this emulator. Blank means any file under ROM PATH is considered to be used with this emulator"));
    };
};


class ScreenPath: virtual public MGSetting, virtual public LineEditSetting {
public:
    ScreenPath(const MythGamePlayerSettings& parent):
        MGSetting(parent, "screenshots") {
        setLabel(QObject::tr("ScreenShots"));
        setHelpText(QObject::tr("Path to any screenshots for this player"));
    };
};

MythGamePlayerSettings::MythGamePlayerSettings()
{
    // must be first
    addChild(id = new ID());
    
    ConfigurationGroup *group = new VerticalConfigurationGroup(false, false);
    group->setLabel(QObject::tr("Game Player Setup"));
    group->addChild(name = new Name(*this));
    group->addChild(new GameType(*this));
    group->addChild(new Command(*this));
    group->addChild(new RomPath(*this));
    group->addChild(new Extensions(*this));
    group->addChild(new ScreenPath(*this));
    group->addChild(new WorkingDirPath(*this));
    addChild(group);
};

void MythGamePlayerSettings::fillSelections(SelectSetting* setting)
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT playername, gameplayerid, gametype FROM gameplayers WHERE playername <> '' ORDER BY playername;");

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        while (result.next())
        {
            setting->addSelection(result.value(0).toString() + " (" + result.value(2).toString() + ")",
                                  result.value(1).toString());
        }
    }
}

void MythGamePlayerSettings::loadByID(int sourceid)
{
    id->setValue(sourceid);
    load();
}

int MythGamePlayerEditor::exec() {
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        edit();

    return QDialog::Rejected;
}

void MythGamePlayerEditor::load() {
    clearSelections();
    addSelection(QObject::tr("(New Game Player)"), "0");
    MythGamePlayerSettings::fillSelections(this);
}

MythDialog* MythGamePlayerEditor::dialogWidget(MythMainWindow* parent,
                                            const char* widgetName)
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(menu()));
    connect(dialog, SIGNAL(editButtonPressed()), this, SLOT(edit()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(del()));
    return dialog;
}

void MythGamePlayerEditor::menu(void)
{
    if (getValue().toInt() == 0)
    {   
        MythGamePlayerSettings gp;
        gp.exec();
    }
    else
    {   
        int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                                 "",
                                                 tr("Game Player Menu"),
                                                 tr("Edit.."),                                                                   tr("Delete.."), 1);

        if (val == 0)
            edit();
        else if (val == 1)
            del();
    }
}

void MythGamePlayerEditor::edit(void)                                                  {   
    MythGamePlayerSettings gp;
    if (getValue().toInt() != 0)
        gp.loadByID(getValue().toInt());

   gp.exec();
}

void MythGamePlayerEditor::del(void)
{
    int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(), "",
                                          tr("Are you sure you want to delete "
                                             "this item?"),
                                             tr("Yes, delete It"),
                                             tr("No, don't"), 2);

    if (val == 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM gameplayers WHERE gameplayerid= :SOURCEID ;");
        query.bindValue(":SOURCEID", getValue());

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Deleting MythGamePlayerSettings:", query);

        load(); 
    }
}

