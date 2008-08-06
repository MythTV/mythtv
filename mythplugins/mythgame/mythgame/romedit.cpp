#include <qstring.h>

#include <mythtv/mythcontext.h>

#include "romedit.h"

class Gamename : public LineEditSetting, public GenericDBStorage
{
  public:
    Gamename(const QString &romname) :
        LineEditSetting(this), GenericDBStorage(
            this, "gamemetadata", "gamename", "romname", romname)
    {
        setLabel(QObject::tr("Game Name"));
        setHelpText(QObject::tr("Friendly name of this Game."));
    }
};

class Genre : public LineEditSetting, public GenericDBStorage
{
  public:
    Genre(const QString &romname) :
        LineEditSetting(this), GenericDBStorage(
            this, "gamemetadata", "genre", "romname", romname)
    {
        setLabel(QObject::tr("Genre"));
        setHelpText(QObject::tr("Genre/Category this game falls under."));
    }
};

class Year : public LineEditSetting, public GenericDBStorage
{
  public:
    Year(const QString &romname) :
        LineEditSetting(this), GenericDBStorage(
            this, "gamemetadata", "year", "romname", romname)
    {
        setLabel(QObject::tr("Year"));
        setHelpText(QObject::tr("The Year the game was released."));
    }
};

class Country : public LineEditSetting, public GenericDBStorage
{
  public:
    Country(const QString &romname) :
        LineEditSetting(this), GenericDBStorage(
            this, "gamemetadata", "country", "romname", romname)
    {
        setLabel(QObject::tr("Country"));
        setHelpText(QObject::tr("The Country this game was "
                                "originally distributed in."));
    }
};

class Publisher : public LineEditSetting, public GenericDBStorage
{
  public:
    Publisher(const QString &romname) :
        LineEditSetting(this), GenericDBStorage(
            this, "gamemetadata", "publisher", "romname", romname)
    {
        setLabel(QObject::tr("Publisher"));
        setHelpText(QObject::tr("The Company that made and "
                                "published this game."));
    }
};


class Favourite : public CheckBoxSetting, public GenericDBStorage
{
  public:
    Favourite(const QString &romname) :
        CheckBoxSetting(this), GenericDBStorage(
            this, "gamemetadata", "favorite", "romname", romname)
    {
        setLabel(QObject::tr("Favorite"));
        setHelpText(QObject::tr("ROM status as a Favorite"));
    }
};

GameEditDialog::GameEditDialog(const QString &romname)
{
    QString title = QObject::tr("Editing Metadata - ") + romname;

    VerticalConfigurationGroup *group =
        new VerticalConfigurationGroup(false);

    group->setLabel(title);
    group->addChild(new Gamename(romname));
    group->addChild(new Genre(romname));
    group->addChild(new Year(romname));
    group->addChild(new Country(romname));
    group->addChild(new Publisher(romname));
    group->addChild(new Favourite(romname));
    addChild(group);
}


