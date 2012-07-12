#ifndef WELCOMESETTINGS_H
#define WELCOMESETTINGS_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include <mythconfigdialogs.h>

class MythWelcomeSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(MythWelcomeSettings)

  public:
    MythWelcomeSettings();
};

class MythShutdownSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(MythShutdownSettings)

  public:
    MythShutdownSettings();
};

#endif
