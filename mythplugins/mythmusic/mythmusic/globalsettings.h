#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

class MusicGeneralSettings: virtual public ConfigurationWizard {
public:
    MusicGeneralSettings();
};

class MusicPlayerSettings: virtual public ConfigurationWizard {
public:
    MusicPlayerSettings();
};

class MusicRipperSettings: virtual public ConfigurationWizard {
public:
    MusicRipperSettings();
};

#endif
