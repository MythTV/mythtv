#ifndef ATARISETTINGSDLG_H
#define ATARISETTINGSDLG_H

#include <mythtv/settings.h>

class AtariSettingsDlg : virtual public ConfigurationWizard {
public:
    AtariSettingsDlg(QString romname);
};

#endif 
