#include <qsqldatabase.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"

const QString currentDatabaseVersion = "1002";

static void UpdateDBVersionNumber(const QString &newnumber)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    db_conn->exec("DELETE FROM settings WHERE value='VideoDBSchemaVer';");
    db_conn->exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('VideoDBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

static void performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    VERBOSE(VB_ALL, QString("Upgrading to MythVideo schema version ") + 
            version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        db_conn->exec(thequery);
        counter++;
        thequery = updates[counter];
    }

    UpdateDBVersionNumber(version);
    dbver = version;
}

static void InitializeDatabase(void)
{
    VERBOSE(VB_ALL, "Inserting MythVideo initial database information.");

    QSqlDatabase *db_conn = QSqlDatabase::database();

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

    QSqlQuery qQuery = db_conn->exec("SELECT * FROM videotypes;");
    if (!qQuery.isActive() || qQuery.numRowsAffected() <= 0)
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
}
