#ifndef ROMEDITDLG_H_
#define ROMEDITDLG_H_

#include <qstring.h>

#include <mythtv/mythcontext.h>
#include <mythtv/settings.h>

class ROMDBStorage : public SimpleDBStorage
{
  public:
    ROMDBStorage(Setting *_setting, QString _name, QString _romname) :
        SimpleDBStorage(_setting, "gamemetadata", _name), romname(_romname)
    {
        _setting->setName(romname);
    }

    virtual QString setClause(MSqlBindings &bindings)
    {
        QString romTag(":SETROMNAME");
        QString colTag(":SET" + getColumn().upper());

        QString query("romname = " + romTag + ", " +
                      getColumn() + " = " + colTag);

        bindings.insert(romTag, romname);
        bindings.insert(colTag, setting->getValue());

        return query;
    }

    virtual QString whereClause(MSqlBindings &bindings)
    {
        QString romTag(":ROMNAME");

        QString query("romname = " + romTag);

        bindings.insert(romTag, romname);

        return query;
    }

    QString romname;
};

class GameEditDialog : public QObject, public ConfigurationDialog
{
  public:
    GameEditDialog(const QString &romname);
};

#endif // ROMEDITDLG_H_
