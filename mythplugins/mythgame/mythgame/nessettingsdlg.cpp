#include "nessettingsdlg.h"

class NoNesSettings: public LabelSetting {
public:
    NoNesSettings(void)
    {
        setLabel("NES settings have not been written yet.");
    };

    virtual void load() {};
    virtual void save() {};
};

NesSettingsDlg::NesSettingsDlg(QString romname)
{
    QString title = tr("NES Game Settings - ") + romname + tr(" - ");

    VerticalConfigurationGroup *first = new VerticalConfigurationGroup(false);
    first->setLabel(title);
    first->addChild(new NoNesSettings());
    addChild(first);
}
    
