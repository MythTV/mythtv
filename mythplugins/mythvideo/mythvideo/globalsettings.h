#ifndef MYTHSETTINGS_H_
#define MYTHSETTINGS_H_

#include <mythtv/settings.h>

class VideoGeneralSettings : public ConfigurationWizard
{
  public:
    VideoGeneralSettings();
};

class DVDRipperSettings :  public ConfigurationWizard
{
  public:
    DVDRipperSettings();
};

#endif
