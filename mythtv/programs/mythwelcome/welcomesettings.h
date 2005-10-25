#ifndef WELCOMESETTINGS_H
#define WELCOMESETTINGS_H

#include <mythtv/settings.h>


class MythWelcomeSettings: virtual public ConfigurationWizard {
public:
    MythWelcomeSettings();
};

class MythShutdownSettings: virtual public ConfigurationWizard {
public:
    MythShutdownSettings();
};

#endif
