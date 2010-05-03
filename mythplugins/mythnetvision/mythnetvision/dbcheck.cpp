// C++ headers
#include <iostream>

// QT headers
#include <QString>
#include <QSqlError>

// Myth headers
#include <mythcontext.h>
#include <mythdb.h>

// MythNetvision headers
#include "dbcheck.h"

const QString currentDatabaseVersion = "1006";

static bool UpdateDBVersionNumber(const QString &newnumber)
{

    if (!gContext->SaveSettingOnHost("NetvisionDBSchemaVer", newnumber, NULL))
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

    VERBOSE(VB_IMPORTANT,
            "Upgrading to MythNetvision schema version " + version);

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

bool UpgradeNetvisionDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("NetvisionDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT,
                "Inserting MythNetvision initial database information.");

        const QString updates[] =
        {
            "CREATE TABLE IF NOT EXISTS netvisionsites "
            "( name VARCHAR(255) NOT NULL PRIMARY KEY,"
            "  thumbnail  VARCHAR(255),"
            "  description TEXT,"
            "  url  TEXT NOT NULL,"
            "  author  VARCHAR(255),"
            "  download BOOL NOT NULL,"
            "  updated TIMESTAMP NOT NULL);",
            ""
        };
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        const QString updates[] =
        {
            "CREATE TABLE IF NOT EXISTS netvisiontreegrabbers "
            "( name VARCHAR(255) NOT NULL,"
            "  thumbnail  VARCHAR(255),"
            "  commandline  TEXT NOT NULL,"
            "  updated TIMESTAMP NOT NULL,"
            "  host  VARCHAR(128));",
            ""
        };
        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }

    if (dbver == "1001")
    {
        const QString updates[] =
        {
            "CREATE TABLE IF NOT EXISTS netvisionsearchgrabbers "
            "( name VARCHAR(255) NOT NULL,"
            "  thumbnail  VARCHAR(255),"
            "  commandline  TEXT NOT NULL,"
            "  host  VARCHAR(128));",
            ""
        };
        if (!performActualUpdate(updates, "1002", dbver))
            return false;
    }

    if (dbver == "1002")
    {
        const QString updates[] =
        {
            "CREATE TABLE IF NOT EXISTS netvisionrssitems "
            "( feedtitle VARCHAR(255) NOT NULL,"
            "  title VARCHAR(255) NOT NULL,"
            "  description TEXT NOT NULL,"
            "  url TEXT NOT NULL,"
            "  thumbnail TEXT NOT NULL,"
            "  mediaURL TEXT NOT NULL,"
            "  author VARCHAR(255) NOT NULL,"
            "  date TIMESTAMP NOT NULL,"
            "  time INT NOT NULL,"
            "  rating VARCHAR(255) NOT NULL,"
            "  filesize BIGINT NOT NULL,"
            "  player VARCHAR(255) NOT NULL,"
            "  playerargs TEXT NOT NULL,"
            "  download VARCHAR(255) NOT NULL,"
            "  downloadargs TEXT NOT NULL,"
            "  width SMALLINT NOT NULL,"
            "  height SMALLINT NOT NULL,"
            "  language  VARCHAR(128),"
            "  downloadable BOOL NOT NULL);",
            ""
        };
        if (!performActualUpdate(updates, "1003", dbver))
            return false;
    }

    if (dbver == "1003")
    {
        const QString updates[] =
        {
            "CREATE TABLE IF NOT EXISTS netvisiontreeitems "
            "( feedtitle VARCHAR(255) NOT NULL,"
            "  path TEXT NOT NULL,"
            "  paththumb TEXT NOT NULL,"
            "  title VARCHAR(255) NOT NULL,"
            "  description TEXT NOT NULL,"
            "  url TEXT NOT NULL,"
            "  thumbnail TEXT NOT NULL,"
            "  mediaURL TEXT NOT NULL,"
            "  author VARCHAR(255) NOT NULL,"
            "  date TIMESTAMP NOT NULL,"
            "  time INT NOT NULL,"
            "  rating VARCHAR(255) NOT NULL,"
            "  filesize BIGINT NOT NULL,"
            "  player VARCHAR(255) NOT NULL,"
            "  playerargs TEXT NOT NULL,"
            "  download VARCHAR(255) NOT NULL,"
            "  downloadargs TEXT NOT NULL,"
            "  width SMALLINT NOT NULL,"
            "  height SMALLINT NOT NULL,"
            "  language VARCHAR(128) NOT NULL,"
            "  downloadable BOOL NOT NULL);",
            ""
        };
        if (!performActualUpdate(updates, "1004", dbver))
            return false;
    }

    if (dbver == "1004")
    {
        const QString updates[] =
        {
            "ALTER TABLE netvisiontreeitems "
            "ADD countries VARCHAR(255) NOT NULL;",
            "ALTER TABLE netvisionrssitems "
            "ADD countries VARCHAR(255) NOT NULL;",
            ""
        };
        if (!performActualUpdate(updates, "1005", dbver))
            return false;
    }

    if (dbver == "1005")
    {
        const QString updates[] =
        {
            "ALTER TABLE netvisiontreeitems "
            "ADD season SMALLINT(5) NOT NULL DEFAULT '0' "
            "AFTER description , ADD episode SMALLINT(5) "
            "NOT NULL DEFAULT '0' AFTER season;",
            "ALTER TABLE netvisionrssitems "
            "ADD season SMALLINT(5) NOT NULL DEFAULT '0' "
            "AFTER description , ADD episode SMALLINT(5) "
            "NOT NULL DEFAULT '0' AFTER season;",
            ""
        };
        if (!performActualUpdate(updates, "1006", dbver))
            return false;
    }

    return true;
}

