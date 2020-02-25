#include <iostream>
using namespace std;

// Qt
#include <QString>
#include <QSqlError>

// MythTV
#include <mythcontext.h>
#include <mythdb.h>
#include <mythdbcheck.h>

// mytharchive
#include "archivedbcheck.h"

const QString currentDatabaseVersion = "1005";
const QString MythArchiveVersionName = "ArchiveDBSchemaVer";

bool UpgradeArchiveDatabaseSchema(void)
{
    QString dbver = gCoreContext->GetSetting("ArchiveDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        LOG(VB_GENERAL, LOG_INFO,
            "Inserting MythArchive initial database information.");

        DBUpdates updates
        {
            "DROP TABLE IF EXISTS archiveitems;",

            "CREATE TABLE IF NOT EXISTS archiveitems ("
            "    intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
            "    type set ('Recording','Video','File'),"
            "    title VARCHAR(128),"
            "    subtitle VARCHAR(128),"
            "    description TEXT,"
            "    startdate VARCHAR(30),"
            "    starttime VARCHAR(30),"
            "    size INT UNSIGNED NOT NULL,"
            "    filename TEXT NOT NULL,"
            "    hascutlist BOOL NOT NULL DEFAULT 0,"
            "    cutlist TEXT,"
            "    INDEX (title)"
            ");"
        };
        if (!performActualUpdate("MythArchive", MythArchiveVersionName,
                                 updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        DBUpdates updates
        {
            "ALTER TABLE archiveitems MODIFY size BIGINT UNSIGNED NOT NULL;"
        };

        if (!performActualUpdate("MythArchive", MythArchiveVersionName,
                                 updates, "1001", dbver))
            return false;
    }


    if (dbver == "1001")
    {
        DBUpdates updates
        {
            qPrintable(QString("ALTER DATABASE %1 DEFAULT CHARACTER SET latin1;")
                       .arg(gContext->GetDatabaseParams().m_dbName)),
            "ALTER TABLE archiveitems"
            "  MODIFY title varbinary(128) default NULL,"
            "  MODIFY subtitle varbinary(128) default NULL,"
            "  MODIFY description blob,"
            "  MODIFY startdate varbinary(30) default NULL,"
            "  MODIFY starttime varbinary(30) default NULL,"
            "  MODIFY filename blob,"
            "  MODIFY cutlist blob;"
        };

        if (!performActualUpdate("MythArchive", MythArchiveVersionName,
                                 updates, "1002", dbver))
            return false;
    }


    if (dbver == "1002")
    {
        DBUpdates updates
        {
            qPrintable(QString("ALTER DATABASE %1 DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;")
                       .arg(gContext->GetDatabaseParams().m_dbName)),
            "ALTER TABLE archiveitems"
            "  DEFAULT CHARACTER SET default,"
            "  MODIFY title varchar(128) CHARACTER SET utf8 default NULL,"
            "  MODIFY subtitle varchar(128) CHARACTER SET utf8 default NULL,"
            "  MODIFY description text CHARACTER SET utf8,"
            "  MODIFY startdate varchar(30) CHARACTER SET utf8 default NULL,"
            "  MODIFY starttime varchar(30) CHARACTER SET utf8 default NULL,"
            "  MODIFY filename text CHARACTER SET utf8 NOT NULL,"
            "  MODIFY cutlist text CHARACTER SET utf8;"
        };

        if (!performActualUpdate("MythArchive", MythArchiveVersionName,
                                 updates, "1003", dbver))
            return false;
    }

    if (dbver == "1003")
    {
        DBUpdates updates
        {
            "ALTER TABLE `archiveitems` "
            "ADD duration INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD cutduration INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD videowidth INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD videoheight INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD filecodec VARCHAR(50) NOT NULL DEFAULT '', "
            "ADD videocodec VARCHAR(50) NOT NULL DEFAULT '', "
            "ADD encoderprofile VARCHAR(50) NOT NULL DEFAULT 'NONE';"
        };

        if (!performActualUpdate("MythArchive", MythArchiveVersionName,
                                 updates, "1004", dbver))
            return false;
    }

    if (dbver == "1004")
    {
        DBUpdates updates
        {
            "DELETE FROM keybindings "
            " WHERE action = 'DELETEITEM' AND context = 'Archive';"
        };

        if (!performActualUpdate("MythArchive", MythArchiveVersionName,
                                 updates, "1005", dbver))
            return false;
    }

    return true;
}

