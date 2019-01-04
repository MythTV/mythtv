#ifndef WELCOMESETTINGS_H
#define WELCOMESETTINGS_H

// MythTV headers
#include <standardsettings.h>

class MythWelcomeSettings : public GroupSetting
{
  public:
    MythWelcomeSettings();
};

class MythShutdownSettings : public GroupSetting
{
  public:
    MythShutdownSettings();
};

#endif
