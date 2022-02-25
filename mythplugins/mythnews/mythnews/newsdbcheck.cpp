// C++ headers
#include <iostream>

// QT headers
#include <QSqlError>
#include <QString>

// Myth headers
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdbcheck.h>

// MythNews headers
#include "newsdbcheck.h"

const QString currentDatabaseVersion = "1001";
const QString MythNewsVersionName = "NewsDBSchemaVer";

bool UpgradeNewsDatabaseSchema(void)
{
    QString dbver = gCoreContext->GetSetting("NewsDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver.isEmpty())
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "Inserting MythNews initial database information.");

        DBUpdates updates
        {
            "CREATE TABLE IF NOT EXISTS newssites "
            "( name VARCHAR(100) NOT NULL PRIMARY KEY,"
            "  category  VARCHAR(255) NOT NULL,"
            "  url  VARCHAR(255) NOT NULL,"
            "  ico  VARCHAR(255),"
            "  updated INT UNSIGNED);"
        };
        if (!performActualUpdate("MythNews", MythNewsVersionName,
                                 updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        DBUpdates updates
        {
            "ALTER TABLE `newssites` ADD `podcast` BOOL NOT NULL DEFAULT '0';"
        };

        if (!performActualUpdate("MythNews", MythNewsVersionName,
                                 updates, "1001", dbver))
            return false;
    }

    return true;
}

