#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

// The real work.

// Temporary dumping ground for things that have not been properly categorized yet
class VideoGeneralSettings: virtual public ConfigurationWizard {
public:
    VideoGeneralSettings();
};

class VideoPlayerSettings: virtual public ConfigurationWizard {
public:
    VideoPlayerSettings();
};

#endif
