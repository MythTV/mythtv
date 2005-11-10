#include <qstring.h>

#include <mythtv/mythcontext.h>

#include "romedit.h"

class RomGamename: virtual public RomSetting, virtual public LineEditSetting {
public:
    RomGamename(QString rom):
        RomSetting("gamename",rom) {
        setLabel(QObject::tr("Game Name"));
        setHelpText(QObject::tr("Friendly name of this Game."));
    };
};

class RomGenre: virtual public RomSetting, virtual public LineEditSetting {
public:
    RomGenre(QString rom):
        RomSetting("genre",rom) {
        setLabel(QObject::tr("Genre"));
        setHelpText(QObject::tr("Genre/Category this game falls under."));
    };
};

class RomYear: virtual public RomSetting, virtual public LineEditSetting {
public:
    RomYear(QString rom):
        RomSetting("year",rom) {
        setLabel(QObject::tr("Year"));
        setHelpText(QObject::tr("The Year the game was released."));
    };
};

class RomCountry: virtual public RomSetting, virtual public LineEditSetting {
public:
    RomCountry(QString rom):
        RomSetting("country",rom) {
        setLabel(QObject::tr("Country"));
        setHelpText(QObject::tr("The Country this game was originally distributed in."));
    };
};

class RomPublisher: virtual public RomSetting, virtual public LineEditSetting {
public:
    RomPublisher(QString rom):
        RomSetting("publisher",rom) {
        setLabel(QObject::tr("Publisher"));
        setHelpText(QObject::tr("The Company that made and published this game."));
    };
};



RomEditDLG::RomEditDLG(QString romname)
{
    QString title = QObject::tr("Editing Metadata - ") + romname;

    VerticalConfigurationGroup *group = new VerticalConfigurationGroup(false, false);
    group->setLabel(title);
    group->addChild(new RomGamename(romname));
    group->addChild(new RomGenre(romname));
    group->addChild(new RomYear(romname));
    group->addChild(new RomCountry(romname));
    group->addChild(new RomPublisher(romname));

    addChild(group);
}


