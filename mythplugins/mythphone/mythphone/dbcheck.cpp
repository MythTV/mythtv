#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

const QString currentDatabaseVersion = "1001";

static bool UpdateDBVersionNumber(const QString &newnumber)
{
    if (!gContext->SaveSettingOnHost("PhoneDBSchemaVer",newnumber,NULL))
    {
        VERBOSE(VB_IMPORTANT, QString("DB Error (Setting new DB version number): %1\n")
                              .arg(newnumber));

        return false;
    }

    return true;
}

static bool performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythPhone schema version ") +
            version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        query.prepare(thequery);
        query.exec();

        if (query.lastError().type() != QSqlError::None)
        {
            QString msg =
                QString("DB Error (Performing database upgrade): \n"
                        "Query was: %1 \nError was: %2 \nnew version: %3")
                .arg(thequery)
                .arg(MythContext::DBErrorMessage(query.lastError()))
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

bool UpgradePhoneDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("PhoneDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT, "Inserting MythPhone initial database information.");

        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS phonedirectory ("
"    intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    nickname TEXT NOT NULL,"
"    firstname TEXT,"
"    surname TEXT,"
"    url TEXT NOT NULL,"
"    directory TEXT NOT NULL,"
"    photofile TEXT,"
"    speeddial INT UNSIGNED NOT NULL"
");",
"CREATE TABLE IF NOT EXISTS phonecallhistory ("
"    recid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    displayname TEXT NOT NULL,"
"    url TEXT NOT NULL,"
"    timestamp TEXT NOT NULL,"
"    duration INT UNSIGNED NOT NULL,"
"    directionin INT UNSIGNED NOT NULL,"
"    directoryref INT UNSIGNED"
");",
""
};
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        const QString updates[] = {
"ALTER TABLE phonedirectory ADD onhomelan INT UNSIGNED DEFAULT 0;"
,
""
};
        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }

    return true;
}

