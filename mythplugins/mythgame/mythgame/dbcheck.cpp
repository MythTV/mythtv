#include <iostream>
using namespace std;

#include <QString>
#include <QSqlError>

#include <mythcontext.h>
#include <mythdb.h>

#include "dbcheck.h"
#include "gamesettings.h"

const QString currentDatabaseVersion = "1016";

static bool UpdateDBVersionNumber(const QString &newnumber)
{

    if (!gCoreContext->SaveSettingOnHost("GameDBSchemaVer",newnumber,NULL))
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

    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythGame schema version ") +
            version);

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
    QString dbver = gCoreContext->GetSetting("GameDBSchemaVer");
    MSqlQuery query(MSqlQuery::InitCon());

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver.isEmpty())
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

        if (!gCoreContext->GetSetting("GameAllTreeLevels").isEmpty())
            if (!query.exec("UPDATE settings SET data = 'system gamename' "
                            "WHERE value = 'GameAllTreeLevels'; "))
                MythDB::DBError("update GameAllTreeLevels", query);

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


    if (dbver == "1012")
    {
        const QString updates[] = {
QString("ALTER DATABASE %1 DEFAULT CHARACTER SET latin1;")
        .arg(gContext->GetDatabaseParams().dbName),
"ALTER TABLE gamemetadata"
"  MODIFY system varbinary(128) NOT NULL default '',"
"  MODIFY romname varbinary(128) NOT NULL default '',"
"  MODIFY gamename varbinary(128) NOT NULL default '',"
"  MODIFY genre varbinary(128) NOT NULL default '',"
"  MODIFY year varbinary(10) NOT NULL default '',"
"  MODIFY publisher varbinary(128) NOT NULL default '',"
"  MODIFY rompath varbinary(255) NOT NULL default '',"
"  MODIFY gametype varbinary(64) NOT NULL default '',"
"  MODIFY country varbinary(128) NOT NULL default '',"
"  MODIFY crc_value varbinary(64) NOT NULL default '',"
"  MODIFY version varbinary(64) NOT NULL default '';",
"ALTER TABLE gameplayers"
"  MODIFY playername varbinary(64) NOT NULL default '',"
"  MODIFY workingpath varbinary(255) NOT NULL default '',"
"  MODIFY rompath varbinary(255) NOT NULL default '',"
"  MODIFY screenshots varbinary(255) NOT NULL default '',"
"  MODIFY commandline blob NOT NULL,"
"  MODIFY gametype varbinary(64) NOT NULL default '',"
"  MODIFY extensions varbinary(128) NOT NULL default '';",
"ALTER TABLE romdb"
"  MODIFY crc varbinary(64) NOT NULL default '',"
"  MODIFY name varbinary(128) NOT NULL default '',"
"  MODIFY description varbinary(128) NOT NULL default '',"
"  MODIFY category varbinary(128) NOT NULL default '',"
"  MODIFY year varbinary(10) NOT NULL default '',"
"  MODIFY manufacturer varbinary(128) NOT NULL default '',"
"  MODIFY country varbinary(128) NOT NULL default '',"
"  MODIFY publisher varbinary(128) NOT NULL default '',"
"  MODIFY platform varbinary(64) NOT NULL default '',"
"  MODIFY flags varbinary(64) NOT NULL default '',"
"  MODIFY version varbinary(64) NOT NULL default '',"
"  MODIFY binfile varbinary(64) NOT NULL default '';",
""
};

        if (!performActualUpdate(updates, "1013", dbver))
            return false;
    }


    if (dbver == "1013")
    {
        const QString updates[] = {
QString("ALTER DATABASE %1 DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;")
        .arg(gContext->GetDatabaseParams().dbName),
"ALTER TABLE gamemetadata"
"  DEFAULT CHARACTER SET default,"
"  MODIFY system varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY romname varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY gamename varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY genre varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY year varchar(10) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY publisher varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY rompath varchar(255) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY gametype varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY country varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY crc_value varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY version varchar(64) CHARACTER SET utf8 NOT NULL default '';",
"ALTER TABLE gameplayers"
"  DEFAULT CHARACTER SET default,"
"  MODIFY playername varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY workingpath varchar(255) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY rompath varchar(255) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY screenshots varchar(255) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY commandline text CHARACTER SET utf8 NOT NULL,"
"  MODIFY gametype varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY extensions varchar(128) CHARACTER SET utf8 NOT NULL default '';",
"ALTER TABLE romdb"
"  DEFAULT CHARACTER SET default,"
"  MODIFY crc varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY name varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY description varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY category varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY year varchar(10) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY manufacturer varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY country varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY publisher varchar(128) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY platform varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY flags varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY version varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY binfile varchar(64) CHARACTER SET utf8 NOT NULL default '';",
""
};

        if (!performActualUpdate(updates, "1014", dbver))
            return false;
    }

    if (dbver == "1014")
    {
        const QString updates[] = {

"ALTER TABLE gamemetadata ADD fanart VARCHAR(255) NOT NULL AFTER rompath,"
"ADD boxart VARCHAR( 255 ) NOT NULL AFTER fanart;",
""
};

        if (!performActualUpdate(updates, "1015", dbver))
            return false;
    }

    if (dbver == "1015")
    {
        const QString updates[] = {

"ALTER TABLE gamemetadata ADD screenshot VARCHAR(255) NOT NULL AFTER rompath,"
"ADD plot TEXT NOT NULL AFTER fanart;",
""
};

        if (!performActualUpdate(updates, "1016", dbver))
            return false;
    }

    return true;
}
