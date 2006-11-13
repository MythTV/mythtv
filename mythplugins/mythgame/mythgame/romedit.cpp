#include <qstring.h>

#include <mythtv/mythcontext.h>

#include "romedit.h"

class Gamename : public LineEditSetting, public ROMDBStorage
{
  public:
    Gamename(const QString &romname) :
        LineEditSetting(this), ROMDBStorage(this, "gamename", romname)
    {
        setLabel(QObject::tr("Game Name"));
        setHelpText(QObject::tr("Friendly name of this Game."));
    }
};

class Genre : public LineEditSetting, public ROMDBStorage
{
  public:
    Genre(const QString &romname) :
        LineEditSetting(this), ROMDBStorage(this, "genre", romname)
    {
        setLabel(QObject::tr("Genre"));
        setHelpText(QObject::tr("Genre/Category this game falls under."));
    }
};

class Year : public LineEditSetting, public ROMDBStorage
{
  public:
    Year(const QString &romname) :
        LineEditSetting(this), ROMDBStorage(this, "year", romname)
    {
        setLabel(QObject::tr("Year"));
        setHelpText(QObject::tr("The Year the game was released."));
    }
};

class Country : public LineEditSetting, public ROMDBStorage
{
  public:
    Country(const QString &romname) :
        LineEditSetting(this), ROMDBStorage(this, "country", romname)
    {
        setLabel(QObject::tr("Country"));
        setHelpText(QObject::tr("The Country this game was "
                                "originally distributed in."));
    }
};

class Publisher : public LineEditSetting, public ROMDBStorage
{
  public:
    Publisher(const QString &romname) :
        LineEditSetting(this), ROMDBStorage(this, "publisher", romname)
    {
        setLabel(QObject::tr("Publisher"));
        setHelpText(QObject::tr("The Company that made and "
                                "published this game."));
    }
};


class Favourite : public CheckBoxSetting, public ROMDBStorage
{
  public:
    Favourite(const QString &romname) :
        CheckBoxSetting(this), ROMDBStorage(this, "favorite", romname)
    {
        setLabel(QObject::tr("Favorite"));
        setHelpText(QObject::tr("ROM status as a Favorite"));
    }
};

GameEditDialog::GameEditDialog(const QString &romname)
{
    QString title = QObject::tr("Editing Metadata - ") + romname;

    VerticalConfigurationGroup *group =
        new VerticalConfigurationGroup(false, false, false, false);

    group->setLabel(title);
    group->addChild(new Gamename(romname));
    group->addChild(new Genre(romname));
    group->addChild(new Year(romname));
    group->addChild(new Country(romname));
    group->addChild(new Publisher(romname));
    group->addChild(new Favourite(romname));
    addChild(group);
}


