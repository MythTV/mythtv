#include "pcsettingsdlg.h"

class NoPCSettings: public LabelSetting {
public:
    NoPCSettings(void)
    {
        setLabel("PC game settings have not been written yet.");
    };

    virtual void load(QSqlDatabase *db) { (void)db; };
    virtual void save(QSqlDatabase *db) { (void)db; };
};

PCSettingsDlg::PCSettingsDlg(QString romname)
{
    QString title = "PC Game Settings - " + romname + " - ";

    VerticalConfigurationGroup *first = new VerticalConfigurationGroup(false);
    first->setLabel(title);
    first->addChild(new NoPCSettings());
    addChild(first);
}
    
