#include <qsqldatabase.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"

const QString currentDatabaseVersion = "1001";

static void UpdateDBVersionNumber(const QString &newnumber)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    db_conn->exec("DELETE FROM settings WHERE value='MusicDBSchemaVer';");
    db_conn->exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('MusicDBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

static void performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    VERBOSE(VB_ALL, QString("Upgrading to MythMusic schema version ") + 
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

void UpgradeMusicDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("MusicDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        VERBOSE(VB_ALL, "Inserting MythMusic initial database information.");

        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS musicmetadata ("
"    intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    artist VARCHAR(128) NOT NULL,"
"    album VARCHAR(128) NOT NULL,"
"    title VARCHAR(128) NOT NULL,"
"    genre VARCHAR(128) NOT NULL,"
"    year INT UNSIGNED NOT NULL,"
"    tracknum INT UNSIGNED NOT NULL,"
"    length INT UNSIGNED NOT NULL,"
"    filename TEXT NOT NULL,"
"    rating INT UNSIGNED NOT NULL DEFAULT 5,"
"    lastplay TIMESTAMP NOT NULL,"
"    playcount INT UNSIGNED NOT NULL DEFAULT 0,"
"    INDEX (artist),"
"    INDEX (album),"
"    INDEX (title),"
"    INDEX (genre)"
");",
"CREATE TABLE IF NOT EXISTS musicplaylist ("
"    playlistid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    name VARCHAR(128) NOT NULL,"
"    hostname VARCHAR(255),"
"    songlist TEXT NOT NULL"
");",
""
};
        performActualUpdate(updates, "1000", dbver);
    }

    if (dbver == "1000")
    {
        QString startdir = gContext->GetSetting("MusicLocation");
        if (!startdir.endsWith("/"))
            startdir += "/";

        QSqlDatabase *db_conn = QSqlDatabase::database();
        // urls as filenames are NOT officially supported yet
        QSqlQuery query("SELECT filename, intid FROM musicmetadata WHERE "
                        "filename NOT LIKE ('%://%');", db_conn);
        QSqlQuery modify;

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            int i = 0;
            QString intid, name, newname;

            while (query.next())
            {
                name = query.value(0).toString();
                newname = name;
                intid = query.value(1).toString();

                if (newname.startsWith(startdir))
                { 
                    newname.remove(0, startdir.length());
                    modify.exec(QString("UPDATE musicmetadata SET "
                                "filename = \"%1\" "
                                "WHERE filename = \"%2\" AND intid = %3;")
                                .arg(newname).arg(name).arg(intid));
                    if (modify.isActive())
                        i += modify.numRowsAffected();
                }
            }
            VERBOSE(VB_ALL, QString("Modified %1 entries for db schema 1001").arg(i));
        }

        const QString updates[] = {
""
};
        performActualUpdate(updates, "1001", dbver);
    }

}

