#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

const QString currentDatabaseVersion = "1000";

static bool UpdateDBVersionNumber(const QString &newnumber)
{

    if (!gContext->SaveSettingOnHost("GalleryDBSchemaVer",newnumber,NULL))
    {
        VERBOSE(VB_IMPORTANT, QString("DB Error (Setting new DB version number): %1\n")
                              .arg(newnumber));

        return false;
    }

    return true;
}

static void performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythGallery schema version ") + 
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

void UpgradeGalleryDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("GalleryDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT, "Inserting MythGallery initial database information.");

        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS gallerymetadata ("
"  image VARCHAR(255) NOT NULL PRIMARY KEY,"
"  angle INTEGER NOT NULL"
");",
"INSERT INTO settings VALUES ('GalleryDBSchemaVer', 1000, NULL);",
""
};
        performActualUpdate(updates, "1000", dbver);
    }
}
