/*
    archivesettings.
*/

#ifndef ARCHIVESETTINGS_H
#define ARCHIVESETTINGS_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include <libmythui/standardsettings.h>


class ArchiveSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(ArchiveSettings);

  public:
    ArchiveSettings();
};

#endif
