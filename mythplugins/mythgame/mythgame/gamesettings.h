#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

// The real work.

class MythGameSettings: virtual public ConfigurationWizard {
public:
    MythGameSettings();
};

#endif
