#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

const QString currentDatabaseVersion = "1002";

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

static bool performActualUpdate(const QString updates[], QString version,
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

bool UpgradeGalleryDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("GalleryDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return true;

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
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }



    if (dbver == "1000")
    {
        const QString updates[] = {
QString("ALTER DATABASE %1 DEFAULT CHARACTER SET latin1;")
        .arg(gContext->GetDatabaseParams().dbName),
"ALTER TABLE gallerymetadata"
"  MODIFY image varbinary(255) NOT NULL;",
""
};

        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }


    if (dbver == "1001")
    {
        const QString updates[] = {
QString("ALTER DATABASE %1 DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;")
        .arg(gContext->GetDatabaseParams().dbName),
"ALTER TABLE gallerymetadata"
"  DEFAULT CHARACTER SET default,"
"  MODIFY image varchar(255) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL;",
""
};

        if (!performActualUpdate(updates, "1002", dbver))
            return false;
    }

    return true;
}
