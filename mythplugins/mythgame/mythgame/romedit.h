#ifndef ROMEDITDLG_H_
#define ROMEDITDLG_H_

#include <qstring.h>

#include <mythtv/mythcontext.h>
#include <mythtv/settings.h>

class ROMDBStorage : public SimpleDBStorage
{
  public:
    ROMDBStorage(StorageUser *_user, QString _name, QString _romname) :
        SimpleDBStorage(_user, "gamemetadata", _name), romname(_romname)
    {
    }

    virtual QString GetSetClause(MSqlBindings &bindings) const
    {
        QString romTag(":SETROMNAME");
        QString colTag(":SET" + GetColumnName().upper());

        QString query("romname = " + romTag + ", " +
                      GetColumnName() + " = " + colTag);

        bindings.insert(romTag, romname);
        bindings.insert(colTag, user->GetValue());

        return query;
    }

    virtual QString GetWhereClause(MSqlBindings &bindings) const
    {
        QString romTag(":ROMNAME");

        QString query("romname = " + romTag);

        bindings.insert(romTag, romname);

        return query;
    }

    QString romname;
};

class GameEditDialog : public ConfigurationWizard
{
  public:
    GameEditDialog(const QString &romname);
};

#endif // ROMEDITDLG_H_
