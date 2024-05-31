#ifndef WELCOMESETTINGS_H
#define WELCOMESETTINGS_H

// MythTV headers
#include "libmythui/standardsettings.h"

class MythWelcomeSettings : public GroupSetting
{
    Q_OBJECT
  public:
    MythWelcomeSettings();
};

class MythShutdownSettings : public GroupSetting
{
    Q_OBJECT
public:
    MythShutdownSettings();
};

#endif
