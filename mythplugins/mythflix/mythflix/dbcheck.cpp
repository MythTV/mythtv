#include <qstring.h>
#include <qdir.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

const QString currentDatabaseVersion = "1001";

static bool UpdateDBVersionNumber(const QString &newnumber)
{   

    if (!gContext->SaveSettingOnHost("FlixDBSchemaVer",newnumber,NULL))
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

    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythFlix schema version ") + 
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

bool UpgradeFlixDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("FlixDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT, "Inserting MythFlix initial database information.");

        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS netflix ("
"    name VARCHAR(100) NOT NULL PRIMARY KEY,"
"    category  VARCHAR(255) NOT NULL,"
"    url  VARCHAR(255) NOT NULL,"
"    ico  VARCHAR(255),"
"    updated INT UNSIGNED,"
"    is_queue INT UNSIGNED"
");",
""
};
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        const QString updates[] = {
"ALTER TABLE netflix ADD queue VARCHAR(32) NOT NULL DEFAULT '';",
"ALTER TABLE netflix DROP PRIMARY KEY, ADD PRIMARY KEY (name, queue);",
""
};
        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }
    
    return true;
}
