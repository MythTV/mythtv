#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "libmyth/settings.h"
#include "libmyth/mythcontext.h"

class ThemeSelector: public HostImageSelect {
public:
    ThemeSelector();
};

// The real work.

class PlaybackSettings: virtual public ConfigurationWizard {
public:
    PlaybackSettings();
};

class GeneralSettings: virtual public ConfigurationWizard {
public:
    GeneralSettings();
};

class EPGSettings: virtual public ConfigurationWizard {
public:
    EPGSettings();
};

class AppearanceSettings: virtual public ConfigurationWizard {
public:
    AppearanceSettings();
};

class MainGeneralSettings: virtual public ConfigurationWizard {
public:
    MainGeneralSettings();
};

class GeneralRecPrioritiesSettings: virtual public ConfigurationWizard {
public:
    GeneralRecPrioritiesSettings();
};

class XboxSettings: virtual public ConfigurationWizard {
public:
    XboxSettings();
};

#endif
