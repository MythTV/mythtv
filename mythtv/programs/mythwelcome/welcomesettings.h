#ifndef WELCOMESETTINGS_H
#define WELCOMESETTINGS_H

// MythTV headers
#include <standardsettings.h>

class MythWelcomeSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(MythWelcomeSettings)

  public:
    MythWelcomeSettings();
};

class MythShutdownSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(MythShutdownSettings)

  public:
    MythShutdownSettings();
};

#endif
