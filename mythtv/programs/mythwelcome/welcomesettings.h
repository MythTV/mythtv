#ifndef WELCOMESETTINGS_H
#define WELCOMESETTINGS_H

#include <mythconfigdialogs.h>

class MythWelcomeSettings : public ConfigurationWizard
{
  public:
    MythWelcomeSettings();
};

class MythShutdownSettings : public ConfigurationWizard
{
  public:
    MythShutdownSettings();
};

#endif
