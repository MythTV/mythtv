#include "odyssey2settingsdlg.h"

class NoOdyssey2Settings: public LabelSetting {
public:
    NoOdyssey2Settings(void)
    {
        setLabel("Odyssey2 settings have not been written yet.");
    };

    virtual void load() {};
    virtual void save() {};
};

Odyssey2SettingsDlg::Odyssey2SettingsDlg(QString romname)
{
    QString title = tr("Odyssey2 Game Settings - ") + romname + tr(" - ");

    VerticalConfigurationGroup *first = new VerticalConfigurationGroup(false);
    first->setLabel(title);
    first->addChild(new NoOdyssey2Settings());
    addChild(first);
}
    
