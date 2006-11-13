#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "libmyth/settings.h"
#include "libmyth/mythcontext.h"

class ThemeSelector : public HostImageSelect
{
  public:
    ThemeSelector();
};

class PlaybackSettings : public ConfigurationWizard
{
  public:
    PlaybackSettings();
};

class GeneralSettings : public ConfigurationWizard
{
  public:
    GeneralSettings();
};

class EPGSettings : public ConfigurationWizard
{
  public:
    EPGSettings();
};

class AppearanceSettings : public ConfigurationWizard
{
  public:
    AppearanceSettings();
};

class MainGeneralSettings : public ConfigurationWizard
{
  public:
    MainGeneralSettings();
};

class GeneralRecPrioritiesSettings : public ConfigurationWizard
{
  public:
    GeneralRecPrioritiesSettings();
};

class XboxSettings : public ConfigurationWizard
{
  public:
    XboxSettings();
};

#endif
