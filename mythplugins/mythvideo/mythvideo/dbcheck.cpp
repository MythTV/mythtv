#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

const QString currentDatabaseVersion = "1007";

static void UpdateDBVersionNumber(const QString &newnumber)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.exec("DELETE FROM settings WHERE value='VideoDBSchemaVer';");
    query.exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('VideoDBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

static void performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_ALL, QString("Upgrading to MythVideo schema version ") + 
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

static void InitializeDatabase(void)
{
    VERBOSE(VB_ALL, "Inserting MythVideo initial database information.");

    const QString updates[] = {
"CREATE TABLE IF NOT EXISTS videometadata ("
"    intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    title VARCHAR(128) NOT NULL,"
"    director VARCHAR(128) NOT NULL,"
"    plot VARCHAR(255) NOT NULL,"
"    rating VARCHAR(128) NOT NULL,"
"    inetref VARCHAR(32) NOT NULL,"
"    year INT UNSIGNED NOT NULL,"
"    userrating FLOAT NOT NULL,"
"    length INT UNSIGNED NOT NULL,"
"    showlevel INT UNSIGNED NOT NULL,"
"    filename TEXT NOT NULL,"
"    coverfile TEXT NOT NULL,"
"    childid INT NOT NULL DEFAULT -1,"
"    browse BOOL NOT NULL DEFAULT 1,"
"    playcommand VARCHAR(255),"
"    INDEX (director),"
"    INDEX (title)"
");",
"CREATE TABLE IF NOT EXISTS videotypes ("
"    intid       INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    extension   VARCHAR(128) NOT NULL,"
"    playcommand VARCHAR(255) NOT NULL,"
"    f_ignore    BOOL,"
"    use_default BOOL"
");",
""
};
    QString dbver = "";
    performActualUpdate(updates, "1000", dbver);

    MSqlQuery qQuery(MSqlQuery::InitCon());
    qQuery.exec("SELECT * FROM videotypes;");

    if (!qQuery.isActive() || qQuery.size() <= 0)
    {
        const QString updates2[] = {
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
"    VALUES (\"txt\", \"\", 1, 0);",
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
"    VALUES (\"log\", \"\", 1, 0);",
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
"    VALUES (\"mpg\", \"\", 0, 1);",
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
"    VALUES (\"avi\", \"\", 0, 1);",
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
"    VALUES (\"vob\", \"\", 0, 1);",
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
"    VALUES (\"mpeg\", \"\", 0, 1);",
""
};
        dbver = "";
        performActualUpdate(updates2, "1000", dbver);
    }
}

void UpgradeVideoDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("VideoDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        InitializeDatabase();
        dbver = "1000";
    }

    if (dbver == "1000")
    {
        const QString updates[] = {
"ALTER TABLE videometadata ADD playcommand VARCHAR(255);",
"ALTER TABLE videometadata ADD INDEX(title);",
"ALTER TABLE videometadata ADD browse BOOL NOT NULL DEFAULT 1;",
""
};

        performActualUpdate(updates, "1001", dbver);
    }

    if (dbver == "1001")
    {
        const QString updates[] = {
"ALTER TABLE videometadata CHANGE childid childid INT NOT NULL DEFAULT -1;",
""
};

        performActualUpdate(updates, "1002", dbver);
    }
    if (dbver == "1002")
    {
        const QString updates[] = {
"ALTER TABLE videometadata CHANGE plot plot TEXT;",
"ALTER TABLE videometadata ADD COLUMN category INT UNSIGNED NOT NULL DEFAULT 0;",
"CREATE TABLE IF NOT EXISTS videocategory ( intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY, category VARCHAR(128) NOT NULL );",
"CREATE TABLE IF NOT EXISTS videocountry ( intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY, country VARCHAR(128) NOT NULL ); ",
"CREATE TABLE IF NOT EXISTS videometadatacountry ( idvideo INT UNSIGNED NOT NULL, idcountry INT UNSIGNED NOT NULL );",
"CREATE TABLE IF NOT EXISTS videogenre ( intid INT UNSIGNED AUTO_INCREMENT NOT NULL  PRIMARY KEY, genre VARCHAR(128) NOT NULL);",
"CREATE TABLE IF NOT EXISTS videometadatagenre ( idvideo INT UNSIGNED NOT NULL,	idgenre INT UNSIGNED NOT NULL );",
""
};

        performActualUpdate(updates, "1003", dbver);
    }
    
    
    if (dbver == "1003")
    {
        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS filemarkup"
"("
"    filename TEXT NOT NULL,"
"    mark BIGINT(20) NOT NULL,"
"    offset VARCHAR(32) NULL,"
"    type INT NOT NULL"
");",
""
};
        performActualUpdate(updates, "1004", dbver);
    }

    if (dbver == "1004")
    {
        const QString updates[] = {
"UPDATE keybindings SET keylist = \"[,{,F10\" WHERE action = \"DECPARENT\" AND keylist = \"Left\";",
"UPDATE keybindings SET keylist = \"],},F11\" WHERE action = \"INCPARENT\" AND keylist = \"Right\";",
""
};
        performActualUpdate(updates, "1005", dbver);
    }

    if (dbver == "1005")
    {
        const QString updates[] = {
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default) "
"VALUES (\"VIDEO_TS\", \"mplayer -fs -zoom -quiet -vo xv -dvd-device %s dvd://1\", 0, 1);",
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default) "
"VALUES (\"iso\", \"mplayer -fs -zoom -quiet -vo xv -dvd-device %s dvd://1\", 0, 1);",
""
    };
        performActualUpdate(updates, "1006", dbver);
    }

    if (dbver == "1006")
    {
        const QString updates[] = {
"ALTER TABLE videometadatacountry ADD INDEX(idvideo); ", 
"ALTER TABLE videometadatacountry ADD INDEX(idcountry);",  
"ALTER TABLE videometadatagenre ADD INDEX(idvideo);",             
"ALTER TABLE videometadatagenre ADD INDEX(idgenre);",
""
};

        performActualUpdate(updates, "1007", dbver);
    }

}
