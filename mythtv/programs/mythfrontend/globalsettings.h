#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "libmyth/settings.h"

class PlaybackSettings: virtual public ConfigurationWizard {
public:
    PlaybackSettings();
};

// Temporary dumping ground for things that have not been properly categorized yet
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

#endif
