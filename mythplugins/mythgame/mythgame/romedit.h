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

    virtual QString setClause(void)
    {
        return QString("romname = \"%1\", %2 = '%3'")
                      .arg(romname).arg(getColumn()).arg(getValue());
    }

    virtual QString whereClause(void)
    {
        return QString("romname = \"%1\"").arg(romname);
    }

    QString romname;
};

class RomEditDLG: virtual public ConfigurationWizard {
public:
    RomEditDLG(QString romname);
};

#endif


