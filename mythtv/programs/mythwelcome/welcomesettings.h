#ifndef WELCOMESETTINGS_H
#define WELCOMESETTINGS_H

// MythTV headers
#include <standardsettings.h>

class MPUBLIC MythWelcomeSettings : public GroupSetting
{
    Q_OBJECT
  public:
    MythWelcomeSettings();
};

class MPUBLIC MythShutdownSettings : public GroupSetting
{
    Q_OBJECT
public:
    MythShutdownSettings();
};

#endif
