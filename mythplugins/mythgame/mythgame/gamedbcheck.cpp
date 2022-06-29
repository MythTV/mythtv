// C++
#include <iostream>

// Qt
#include <QString>
#include <QSqlError>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdbcheck.h>

// MythGame
#include "gamedbcheck.h"
#include "gamesettings.h"

const QString currentDatabaseVersion = "1020";
const QString MythGameVersionName = "GameDBSchemaVer";

static bool InitializeDatabase(void)
{
    LOG(VB_GENERAL, LOG_NOTICE,
        "Inserting MythGame initial database information.");

    DBUpdates updates {
"CREATE TABLE gamemetadata ("
"  `system` varchar(128) NOT NULL default '',"
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
"  KEY `system` (`system`),"
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
");"
};
    QString dbver = "";
    return performActualUpdate("MythGame", MythGameVersionName,
                               updates, "1011", dbver);
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
        DBUpdates updates {
"ALTER TABLE gamemetadata ADD COLUMN favorite BOOL NULL;"
};
        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1001", dbver))
            return false;
    }

    if ((((dbver == "1004")
      || (dbver == "1003"))
      || (dbver == "1002"))
      || (dbver == "1001"))
    {
        DBUpdates updates {

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
"ALTER TABLE gamemetadata ADD COLUMN gametype varchar(64) NOT NULL default ''; "
};
        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1005", dbver))
            return false;
    }

    if (dbver == "1005")
    {
        DBUpdates updates {
"ALTER TABLE gameplayers ADD COLUMN spandisks tinyint(1) NOT NULL default 0; ",
"ALTER TABLE gamemetadata ADD COLUMN diskcount tinyint(1) NOT NULL default 1; "
};
        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1006", dbver))
            return false;
    }

    if (dbver == "1006")
    {

        if (!gCoreContext->GetSetting("GameAllTreeLevels").isEmpty())
        {
            if (!query.exec("UPDATE settings SET data = 'system gamename' "
                            "WHERE value = 'GameAllTreeLevels'; "))
                MythDB::DBError("update GameAllTreeLevels", query);
        }

        DBUpdates updates = {
"ALTER TABLE gamemetadata ADD COLUMN country varchar(128) NOT NULL default ''; ",
"ALTER TABLE gamemetadata ADD COLUMN crc_value varchar(64) NOT NULL default ''; ",
"ALTER TABLE gamemetadata ADD COLUMN display tinyint(1) NOT NULL default 1; "
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1007", dbver))
            return false;
    }

    if (dbver == "1007")
    {
        DBUpdates updates {
"ALTER TABLE gameplayers MODIFY commandline TEXT NOT NULL default ''; "
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1008", dbver))
            return false;
    }

    if (dbver == "1008")
    {
        DBUpdates updates {
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
");"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1009", dbver))
            return false;
    }

    if (dbver == "1009")
    {
        DBUpdates updates {
"ALTER TABLE gamemetadata MODIFY year varchar(10) not null default '';"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1010", dbver))
            return false;
    }

    if (dbver == "1010")
    {
        DBUpdates updates {

"ALTER TABLE gamemetadata ADD COLUMN version varchar(64) NOT NULL default '';",
"ALTER TABLE gamemetadata ADD COLUMN publisher varchar(128) NOT NULL default '';"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1011", dbver))
            return false;
    }


    if (dbver == "1011")
    {
        DBUpdates updates {
"ALTER TABLE romdb ADD COLUMN binfile varchar(64) NOT NULL default ''; "
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1012", dbver))
            return false;
    }


    if (dbver == "1012")
    {
        DBUpdates updates {
qPrintable(QString("ALTER DATABASE %1 DEFAULT CHARACTER SET latin1;")
        .arg(GetMythDB()->GetDatabaseName())),
"ALTER TABLE gamemetadata"
"  MODIFY `system` varbinary(128) NOT NULL default '',"
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
"  MODIFY binfile varbinary(64) NOT NULL default '';"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1013", dbver))
            return false;
    }


    if (dbver == "1013")
    {
        DBUpdates updates {
qPrintable(QString("ALTER DATABASE %1 DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;")
        .arg(GetMythDB()->GetDatabaseName())),
"ALTER TABLE gamemetadata"
"  DEFAULT CHARACTER SET utf8,"
"  MODIFY `system` varchar(128) CHARACTER SET utf8 NOT NULL default '',"
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
"  DEFAULT CHARACTER SET utf8,"
"  MODIFY playername varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY workingpath varchar(255) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY rompath varchar(255) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY screenshots varchar(255) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY commandline text CHARACTER SET utf8 NOT NULL,"
"  MODIFY gametype varchar(64) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY extensions varchar(128) CHARACTER SET utf8 NOT NULL default '';",
"ALTER TABLE romdb"
"  DEFAULT CHARACTER SET utf8,"
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
"  MODIFY binfile varchar(64) CHARACTER SET utf8 NOT NULL default '';"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1014", dbver))
            return false;
    }

    if (dbver == "1014")
    {
        DBUpdates updates {

"ALTER TABLE gamemetadata ADD fanart VARCHAR(255) NOT NULL AFTER rompath,"
"ADD boxart VARCHAR( 255 ) NOT NULL AFTER fanart;"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1015", dbver))
            return false;
    }

    if (dbver == "1015")
    {
        DBUpdates updates {

"ALTER TABLE gamemetadata ADD screenshot VARCHAR(255) NOT NULL AFTER rompath,"
"ADD plot TEXT NOT NULL AFTER fanart;"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1016", dbver))
            return false;
    }

    if (dbver == "1016")
    {
        DBUpdates updates {

"ALTER TABLE gamemetadata ADD inetref TEXT AFTER crc_value;"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1017", dbver))
            return false;
    }

    if (dbver == "1017")
    {
        DBUpdates updates {

"ALTER TABLE gamemetadata ADD intid int(11) NOT NULL AUTO_INCREMENT "
"PRIMARY KEY FIRST;"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1018", dbver))
            return false;
    }

    if (dbver == "1018")
    {
        DBUpdates updates {
"ALTER TABLE romdb MODIFY description varchar(192) CHARACTER SET utf8 NOT NULL default '';",
"ALTER TABLE romdb MODIFY binfile     varchar(128) CHARACTER SET utf8 NOT NULL default '';",
"ALTER TABLE romdb MODIFY filesize    int(12) unsigned default NULL;"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1019", dbver))
            return false;
    }

    // Repeat 1013 DBs pre MySQL v8 systems that may have not be set to utf8

    if (dbver == "1019")
    {
        DBUpdates updates {
"ALTER TABLE gamemetadata DEFAULT CHARACTER SET utf8;",
"ALTER TABLE gameplayers DEFAULT CHARACTER SET utf8;",
"ALTER TABLE romdb DEFAULT CHARACTER SET utf8;"
};

        if (!performActualUpdate("MythGame", MythGameVersionName,
                                 updates, "1020", dbver))
            return false;
    }

    return true;

}
