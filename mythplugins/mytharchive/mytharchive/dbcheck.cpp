#include <iostream>
using namespace std;

// Qt
#include <QString>
#include <QSqlError>

// MythTV
#include <mythcontext.h>
#include <mythdb.h>

// mytharchive
#include "dbcheck.h"

const QString currentDatabaseVersion = "1005";

static bool UpdateDBVersionNumber(const QString &newnumber)
{

    if (!gCoreContext->SaveSettingOnHost("ArchiveDBSchemaVer",newnumber,NULL))
    {
        VERBOSE(VB_IMPORTANT,
                QString("DB Error (Setting new DB version number): %1\n")
                .arg(newnumber));

        return false;
    }

    return true;
}

static bool performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_IMPORTANT, "Upgrading to MythArchive schema version " + version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        if (!query.exec(thequery))
        {
            QString msg =
                QString("DB Error (Performing database upgrade): \n"
                        "Query was: %1 \nError was: %2 \nnew version: %3")
                .arg(thequery)
                .arg(MythDB::DBErrorMessage(query.lastError()))
                .arg(version);
            VERBOSE(VB_IMPORTANT, msg);
            return false;
        }

        counter++;
        thequery = updates[counter];
    }

    if (!UpdateDBVersionNumber(version))
        return false;

    dbver = version;
    return true;
}

bool UpgradeArchiveDatabaseSchema(void)
{
    QString dbver = gCoreContext->GetSetting("ArchiveDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT,
                "Inserting MythArchive initial database information.");

        const QString updates[] = 
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
            ");",
            ""
        };
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        const QString updates[] =
        {
            "ALTER TABLE archiveitems MODIFY size BIGINT UNSIGNED NOT NULL;",
            ""
        };

        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }


    if (dbver == "1001")
    {
        const QString updates[] =
        {
            QString("ALTER DATABASE %1 DEFAULT CHARACTER SET latin1;")
                    .arg(gContext->GetDatabaseParams().dbName),
            "ALTER TABLE archiveitems"
            "  MODIFY title varbinary(128) default NULL,"
            "  MODIFY subtitle varbinary(128) default NULL,"
            "  MODIFY description blob,"
            "  MODIFY startdate varbinary(30) default NULL,"
            "  MODIFY starttime varbinary(30) default NULL,"
            "  MODIFY filename blob,"
            "  MODIFY cutlist blob;",
            ""
        };

        if (!performActualUpdate(updates, "1002", dbver))
            return false;
    }


    if (dbver == "1002")
    {
        const QString updates[] = 
        {
            QString("ALTER DATABASE %1 DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;")
                    .arg(gContext->GetDatabaseParams().dbName),
            "ALTER TABLE archiveitems"
            "  DEFAULT CHARACTER SET default,"
            "  MODIFY title varchar(128) CHARACTER SET utf8 default NULL,"
            "  MODIFY subtitle varchar(128) CHARACTER SET utf8 default NULL,"
            "  MODIFY description text CHARACTER SET utf8,"
            "  MODIFY startdate varchar(30) CHARACTER SET utf8 default NULL,"
            "  MODIFY starttime varchar(30) CHARACTER SET utf8 default NULL,"
            "  MODIFY filename text CHARACTER SET utf8 NOT NULL,"
            "  MODIFY cutlist text CHARACTER SET utf8;",
            ""
        };

        if (!performActualUpdate(updates, "1003", dbver))
            return false;
    }

    if (dbver == "1003")
    {
        const QString updates[] = 
        {
            "ALTER TABLE `archiveitems` "
            "ADD duration INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD cutduration INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD videowidth INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD videoheight INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD filecodec VARCHAR(50) NOT NULL DEFAULT '', "
            "ADD videocodec VARCHAR(50) NOT NULL DEFAULT '', "
            "ADD encoderprofile VARCHAR(50) NOT NULL DEFAULT 'NONE';",
            ""
        };

        if (!performActualUpdate(updates, "1004", dbver))
            return false;
    }

    if (dbver == "1004")
    {
        const QString updates[] =
        {
            "DELETE FROM keybindings "
            " WHERE action = 'DELETEITEM' AND context = 'Archive';",
            ""
        };

        if (!performActualUpdate(updates, "1005", dbver))
            return false;
    }

    return true;
}

