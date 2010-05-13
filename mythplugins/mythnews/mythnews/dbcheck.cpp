// C++ headers
#include <iostream>

// QT headers
#include <QString>
#include <QSqlError>

// Myth headers
#include <mythcontext.h>
#include <mythdb.h>

// MythNews headers
#include "dbcheck.h"

const QString currentDatabaseVersion = "1001";

static bool UpdateDBVersionNumber(const QString &newnumber)
{

    if (!gCoreContext->SaveSettingOnHost("NewsDBSchemaVer", newnumber, NULL))
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
            "Upgrading to MythNews schema version " + version);

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

bool UpgradeNewsDatabaseSchema(void)
{
    QString dbver = gCoreContext->GetSetting("NewsDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT,
                "Inserting MythNews initial database information.");

        const QString updates[] =
        {
            "CREATE TABLE IF NOT EXISTS newssites "
            "( name VARCHAR(100) NOT NULL PRIMARY KEY,"
            "  category  VARCHAR(255) NOT NULL,"
            "  url  VARCHAR(255) NOT NULL,"
            "  ico  VARCHAR(255),"
            "  updated INT UNSIGNED);",
            ""
        };
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        const QString updates[] =
        {
            "ALTER TABLE `newssites` ADD `podcast` BOOL NOT NULL DEFAULT '0';",
            ""
        };

        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }

    return true;
}

