#ifndef SNESSETTINGSDLG_H_
#define SNESSETTINGSDLG_H_

#include <mythtv/settings.h>

class SnesSettingsDlg: virtual public ConfigurationWizard {
public:
    SnesSettingsDlg(QString romname);
};

#endif
