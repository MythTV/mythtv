#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "libmyth/settings.h"

class MythContext;

class PlaybackSettings: virtual public ConfigurationWizard {
public:
    PlaybackSettings(MythContext *context);
};

// Temporary dumping ground for things that have not been properly categorized yet
class GeneralSettings: virtual public ConfigurationWizard {
public:
    GeneralSettings(MythContext *context);
};

class EPGSettings: virtual public ConfigurationWizard {
public:
    EPGSettings(MythContext* context);
};

class AppearanceSettings: virtual public ConfigurationWizard {
public:
    AppearanceSettings(MythContext* context);
};

#endif
