#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

class MusicGeneralSettings : public ConfigurationWizard
{
  public:
    MusicGeneralSettings();
};

class MusicPlayerSettings : public QObject, public ConfigurationWizard
{
  Q_OBJECT

  public:
    MusicPlayerSettings();
    ~MusicPlayerSettings() {};
};

class MusicRipperSettings : public ConfigurationWizard
{
  public:
    MusicRipperSettings();
};

#endif
