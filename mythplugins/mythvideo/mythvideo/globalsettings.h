#ifndef MYTHSETTINGS_H_
#define MYTHSETTINGS_H_

#include <mythtv/settings.h>

// The real work.

// Temporary dumping ground for things that have not been properly
// categorized yet
class VideoGeneralSettings: virtual public ConfigurationWizard
{
  public:
    VideoGeneralSettings();
};

class VideoPlayerSettings: virtual public ConfigurationWizard
{
  public:
    VideoPlayerSettings();
};

#endif
