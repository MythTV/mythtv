#ifndef MAMESETTINGSDLG_H_
#define MAMESETTINGSDLG_H_

#include <mythtv/settings.h>

class Prefs;

class MameSettingsDlg: virtual public ConfigurationWizard {
public:
    MameSettingsDlg(QString romname, Prefs *prefs);
};

#endif
