#ifndef ROMEDITDLG_H_
#define ROMEDITDLG_H_

#include <qstring.h>

#include <mythtv/mythcontext.h>
#include <mythtv/settings.h>

class RomSetting: public SimpleDBStorage {
public:
    RomSetting(QString name, QString _romname):
        SimpleDBStorage("gamemetadata", name),
        romname(_romname) {
        setName(name);
    }

    virtual QString setClause(MSqlBindings &bindings)
    {
        QString romTag(":SETROMNAME");
        QString colTag(":SET" + getColumn().upper());

        QString query("romname = " + romTag + ", " +
                      getColumn() + " = " + colTag);

        bindings.insert(romTag, romname);
        bindings.insert(colTag, getValue());

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

class RomEditDLG: virtual public ConfigurationWizard {
public:
    RomEditDLG(QString romname);
};

#endif


