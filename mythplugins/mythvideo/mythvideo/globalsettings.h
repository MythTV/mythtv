#ifndef MYTHSETTINGS_H_
#define MYTHSETTINGS_H_

#include <mythtv/settings.h>

class VideoGeneralSettings : public ConfigurationWizard
{
  public:
    VideoGeneralSettings();
};

class VideoPlayerSettings : public ConfigurationWizard
{
  public:
    VideoPlayerSettings();
};

class DVDRipperSettings :  public ConfigurationWizard
{
  public:
    DVDRipperSettings();
};

#endif
