#ifndef PCSETTINGSDLG_H
#define PCSETTINGSDLG_H

#include <mythtv/settings.h>

class PCSettingsDlg : virtual public ConfigurationWizard {
public:
    PCSettingsDlg(QString romname);
};

#endif 
