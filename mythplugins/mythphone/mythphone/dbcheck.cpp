#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

const QString currentDatabaseVersion = "1001";

static void UpdateDBVersionNumber(const QString &newnumber)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.exec("DELETE FROM settings WHERE value='PhoneDBSchemaVer';");
    query.exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('PhoneDBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

static void performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_ALL, QString("Upgrading to MythPhone schema version ") +
            version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        query.exec(thequery);
        counter++;
        thequery = updates[counter];
    }

    UpdateDBVersionNumber(version);
    dbver = version;
}

void UpgradePhoneDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("PhoneDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        VERBOSE(VB_ALL, "Inserting MythPhone initial database information.");

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
        performActualUpdate(updates, "1000", dbver);
    }

    if (dbver == "1000")
    {
        const QString updates[] = {
"ALTER TABLE phonedirectory ADD onhomelan INT UNSIGNED DEFAULT 0;"
,
""
};
        performActualUpdate(updates, "1001", dbver);
    }


}

