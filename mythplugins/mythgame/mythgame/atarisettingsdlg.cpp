#include "atarisettingsdlg.h"

class NoAtariSettings: public LabelSetting {
public:
    NoAtariSettings(void)
    {
        setLabel("Atari settings have not been written yet.");
    };

    virtual void load() {};
    virtual void save() {};
};

AtariSettingsDlg::AtariSettingsDlg(QString romname)
{
    QString title = tr("Atari Game Settings - ") + romname + tr(" - ");

    VerticalConfigurationGroup *first = new VerticalConfigurationGroup(false);
    first->setLabel(title);
    first->addChild(new NoAtariSettings());
    addChild(first);
}
    
