/*
    archivesettings.
*/

#ifndef ARCHIVESETTINGS_H
#define ARCHIVESETTINGS_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include <settings.h>


class ArchiveSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(ArchiveSettings)

  public:
    ArchiveSettings();
};

#endif
