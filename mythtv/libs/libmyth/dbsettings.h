#ifndef DBSETTINGS_H
#define DBSETTINGS_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "mythconfigdialogs.h"

class MPUBLIC DatabaseSettings: public ConfigurationWizard {

    Q_DECLARE_TR_FUNCTIONS(DatabaseSettings)

public:
    DatabaseSettings(const QString &DBhostOverride = QString::null);
    
    // This routine calls wizard->addChild() for each of
    // the database configuration screens.  This allows
    // the number of DB config screens to change.
    static void addDatabaseSettings(ConfigurationWizard *wizard);
};

#endif
