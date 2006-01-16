#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

#include "gamesettings.h"

const QString currentDatabaseVersion = "1012";

static bool UpdateDBVersionNumber(const QString &newnumber)
{
    // delete old schema version
    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery = "DELETE FROM settings WHERE value='GameDBSchemaVer';";
    query.prepare(thequery);
    query.exec();

    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (Deleting old DB version number): \n"
                    "Query was: %1 \nError was: %2 \nnew version: %3")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()))
            .arg(newnumber);
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }

    // set new schema version
    thequery = QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('GameDBSchemaVer', %1, NULL);")
                         .arg(newnumber);
    query.prepare(thequery);
    query.exec();

    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (Setting new DB version number): \n"
                    "Query was: %1 \nError was: %2 \nnew version: %3")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()))
            .arg(newnumber);
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }

    return true;
}

static bool performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythGame schema version ") + 
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

bool InitializeDatabase(void)
{
    VERBOSE(VB_IMPORTANT, "Inserting MythGame initial database information.");

    const QString updates[] = {
"CREATE TABLE gamemetadata ("
"  system varchar(128) NOT NULL default '',"
"  romname varchar(128) NOT NULL default '',"
"  gamename varchar(128) NOT NULL default '',"
"  genre varchar(128) NOT NULL default '',"
"  year varchar(10) NOT NULL default '',"
"  publisher varchar(128) NOT NULL default '',"
"  favorite tinyint(1) default NULL,"
"  rompath varchar(255) NOT NULL default '',"
"  gametype varchar(64) NOT NULL default '',"
"  diskcount tinyint(1) NOT NULL default '1',"
"  country varchar(128) NOT NULL default '',"
"  crc_value varchar(64) NOT NULL default '',"
"  display tinyint(1) NOT NULL default '1',"
"  version varchar(64) NOT NULL default '',"
"  KEY system (system),"
"  KEY year (year),"
"  KEY romname (romname),"
"  KEY gamename (gamename),"
"  KEY genre (genre)"
");",
"CREATE TABLE gameplayers ("
"  gameplayerid int(10) unsigned NOT NULL auto_increment,"
"  playername varchar(64) NOT NULL default '',"
"  workingpath varchar(255) NOT NULL default '',"
"  rompath varchar(255) NOT NULL default '',"
"  screenshots varchar(255) NOT NULL default '',"
"  commandline text NOT NULL,"
"  gametype varchar(64) NOT NULL default '',"
"  extensions varchar(128) NOT NULL default '',"
"  spandisks tinyint(1) NOT NULL default '0',"
"  PRIMARY KEY  (gameplayerid),"
"  UNIQUE KEY playername (playername)"
");",
"CREATE TABLE romdb ("
"  crc varchar(64) NOT NULL default '',"
"  name varchar(128) NOT NULL default '',"
"  description varchar(128) NOT NULL default '',"
"  category varchar(128) NOT NULL default '',"
"  year varchar(10) NOT NULL default '',"
"  manufacturer varchar(128) NOT NULL default '',"
"  country varchar(128) NOT NULL default '',"
"  publisher varchar(128) NOT NULL default '',"
"  platform varchar(64) NOT NULL default '',"
"  filesize int(12) default NULL,"
"  flags varchar(64) NOT NULL default '',"
"  version varchar(64) NOT NULL default '',"
"  KEY crc (crc),"
"  KEY year (year),"
"  KEY category (category),"
"  KEY name (name),"
"  KEY description (description),"
"  KEY platform (platform)"
");",
""
};
    QString dbver = "";
    if (!performActualUpdate(updates, "1011", dbver))
        return false;

    return true;
}

bool UpgradeGameDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("GameDBSchemaVer");
    MSqlQuery query(MSqlQuery::InitCon());
   
    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        if (!InitializeDatabase())
            return false;
        dbver = "1011";
    }

    if (dbver == "1000")
    {
        const QString updates[] = {
"ALTER TABLE gamemetadata ADD COLUMN favorite BOOL NULL;",
""
};
        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }

    if ((((dbver == "1004") 
      || (dbver == "1003")) 
      || (dbver == "1002"))
      || (dbver == "1001"))
    {   
        const QString updates[] = {

"CREATE TABLE gameplayers ("
"  gameplayerid int(10) unsigned NOT NULL auto_increment,"
"  playername varchar(64) NOT NULL default '',"
"  workingpath varchar(255) NOT NULL default '',"
"  rompath varchar(255) NOT NULL default '',"
"  screenshots varchar(255) NOT NULL default '',"
" commandline varchar(255) NOT NULL default '',"
"  gametype varchar(64) NOT NULL default '',"
"  extensions varchar(128) NOT NULL default '',"
"  PRIMARY KEY (gameplayerid),"
"  UNIQUE KEY playername (playername)"
");",
"ALTER TABLE gamemetadata ADD COLUMN rompath varchar(255) NOT NULL default ''; ",
"ALTER TABLE gamemetadata ADD COLUMN gametype varchar(64) NOT NULL default ''; ",
""
};
        if (!performActualUpdate(updates, "1005", dbver))
            return false;
    }

    if (dbver == "1005")
    {   
        const QString updates[] = {
"ALTER TABLE gameplayers ADD COLUMN spandisks tinyint(1) NOT NULL default 0; ",
"ALTER TABLE gamemetadata ADD COLUMN diskcount tinyint(1) NOT NULL default 1; ",
""
};
        if (!performActualUpdate(updates, "1006", dbver))
            return false;
    }

    if (dbver == "1006")
    {   
        
        if (gContext->GetSetting("GameAllTreeLevels"))
            query.exec("UPDATE settings SET data = 'system gamename' WHERE value = 'GameAllTreeLevels'; ");

        QString updates[] = {
"ALTER TABLE gamemetadata ADD COLUMN country varchar(128) NOT NULL default ''; ",
"ALTER TABLE gamemetadata ADD COLUMN crc_value varchar(64) NOT NULL default ''; ",
"ALTER TABLE gamemetadata ADD COLUMN display tinyint(1) NOT NULL default 1; ",
""
};

        if (!performActualUpdate(updates, "1007", dbver))
            return false;
    }

    if (dbver == "1007")
    {
        const QString updates[] = {
"ALTER TABLE gameplayers MODIFY commandline TEXT NOT NULL default ''; ",
""
};

        if (!performActualUpdate(updates, "1008", dbver))
            return false;
    }

    if (dbver == "1008")
    {
        const QString updates[] = {
"CREATE TABLE romdb ("
"  crc varchar(64) NOT NULL default '',"
"  name varchar(128) NOT NULL default '',"
"  description varchar(128) NOT NULL default '',"
"  category varchar(128) NOT NULL default '',"
"  year varchar(10) NOT NULL default '',"
"  manufacturer varchar(128) NOT NULL default '',"
"  country varchar(128) NOT NULL default '',"
"  publisher varchar(128) NOT NULL default '',"
"  platform varchar(64) NOT NULL default '',"
"  filesize int(12) default NULL,"
"  flags varchar(64) NOT NULL default '',"
"  version varchar(64) NOT NULL default '',"
"  KEY crc (crc),"
"  KEY year (year),"
"  KEY category (category),"
"  KEY name (name),"
"  KEY description (description),"
"  KEY platform (platform)"
");",
""
};

        if (!performActualUpdate(updates, "1009", dbver))
            return false;
    }

    if (dbver == "1009")
    {
        const QString updates[] = {
"ALTER TABLE gamemetadata MODIFY year varchar(10) not null default '';",
""
};

        if (!performActualUpdate(updates, "1010", dbver))
            return false;
    }

    if (dbver == "1010")
    {   
        const QString updates[] = {

"ALTER TABLE gamemetadata ADD COLUMN version varchar(64) NOT NULL default '';",
"ALTER TABLE gamemetadata ADD COLUMN publisher varchar(128) NOT NULL default '';",
""
};

        if (!performActualUpdate(updates, "1011", dbver))
            return false;
    }


    if (dbver == "1011")
    {
        const QString updates[] = {
"ALTER TABLE romdb ADD COLUMN binfile varchar(64) NOT NULL default ''; ",
""
};

        if (!performActualUpdate(updates, "1012", dbver))
            return false;
    }





    return true;
}
