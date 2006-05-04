#include <iostream>

// qt
#include <qstring.h>
#include <qdir.h>

// myth
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

// mytharchive
#include "dbcheck.h"


const QString currentDatabaseVersion = "1000";

static void UpdateDBVersionNumber(const QString &newnumber)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.exec("DELETE FROM settings WHERE value='ArchiveDBSchemaVer';");
    query.exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('ArchiveDBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

static void performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythArchive schema version ") + 
            version);

    MSqlQuery query(MSqlQuery::InitCon());

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

void UpgradeArchiveDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("ArchiveDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT, "Inserting MythArchive initial database information.");

        const QString updates[] = {
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
"    cutlist TEXT DEFAULT '',"
"    INDEX (title)"
");",
""
};
        performActualUpdate(updates, "1000", dbver);
    }
}

