#include <qsqldatabase.h>
#include <qstring.h>
#include <qdir.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"

const QString currentDatabaseVersion = "1003";

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
        startdir = QDir::cleanDirPath(startdir);
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

    if (dbver == "1001")
    {
        const QString updates[] = {
"ALTER TABLE musicmetadata ADD mythdigest      VARCHAR(255);",
"ALTER TABLE musicmetadata ADD size            BIGINT UNSIGNED;",
"ALTER TABLE musicmetadata ADD date_added      DATETIME;",
"ALTER TABLE musicmetadata ADD date_modified   DATETIME;",
"ALTER TABLE musicmetadata ADD format          VARCHAR(4);",
"ALTER TABLE musicmetadata ADD description     VARCHAR(255);",
"ALTER TABLE musicmetadata ADD comment         VARCHAR(255);",
"ALTER TABLE musicmetadata ADD compilation     TINYINT DEFAULT 0;",
"ALTER TABLE musicmetadata ADD composer        VARCHAR(255);",
"ALTER TABLE musicmetadata ADD disc_count      SMALLINT UNSIGNED DEFAULT 0;",
"ALTER TABLE musicmetadata ADD disc_number     SMALLINT UNSIGNED DEFAULT 0;",
"ALTER TABLE musicmetadata ADD track_count     SMALLINT UNSIGNED DEFAULT 0;",
"ALTER TABLE musicmetadata ADD start_time      INT UNSIGNED DEFAULT 0;",
"ALTER TABLE musicmetadata ADD stop_time       INT UNSIGNED;",
"ALTER TABLE musicmetadata ADD eq_preset       VARCHAR(255);",
"ALTER TABLE musicmetadata ADD relative_volume TINYINT DEFAULT 0;",
"ALTER TABLE musicmetadata ADD sample_rate     INT UNSIGNED;",
"ALTER TABLE musicmetadata ADD bpm             SMALLINT UNSIGNED;",
"ALTER TABLE musicmetadata ADD INDEX (mythdigest);",
""
};

        performActualUpdate(updates, "1002", dbver);
    }

    if (dbver == "1002")
    {
        VERBOSE(VB_ALL, "Updating music metadata to be UTF-8 in the database");

        QSqlDatabase *db_conn = QSqlDatabase::database();

        QSqlQuery query(QString::null, db_conn);
        query.prepare("SELECT intid, artist, album, title, genre, "
                      "filename FROM musicmetadata ORDER BY intid;");

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next())
            {
                int id = query.value(0).toInt();
                QString artist = query.value(1).toString();
                QString album = query.value(2).toString();
                QString title = query.value(3).toString();
                QString genre = query.value(4).toString();
                QString filename = query.value(5).toString();

                QSqlQuery subquery(QString::null, db_conn);
                subquery.prepare("UPDATE musicmetadata SET "
                                 "artist = :ARTIST, album = :ALBUM, "
                                 "title = :TITLE, genre = :GENRE, "
                                 "filename = :FILENAME "
                                 "WHERE intid = :ID;");
                subquery.bindValue(":ARTIST", artist.utf8());
                subquery.bindValue(":ALBUM", album.utf8());
                subquery.bindValue(":TITLE", title.utf8());
                subquery.bindValue(":GENRE", genre.utf8());
                subquery.bindValue(":FILENAME", filename.utf8());
                subquery.bindValue(":ID", id);

                if (!subquery.exec() || !subquery.isActive())
                    MythContext::DBError("music utf8 update", subquery);
            }
        }

        query.prepare("SELECT playlistid, name FROM musicplaylist "
                      "ORDER BY playlistid;");

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next())
            {
                int id = query.value(0).toInt();
                QString name = query.value(1).toString();

                QSqlQuery subquery(QString::null, db_conn);
                subquery.prepare("UPDATE musicplaylist SET "
                                 "name = :NAME WHERE playlistid = :ID ;");
                subquery.bindValue(":NAME", name.utf8());
                subquery.bindValue(":ID", id);

                if (!subquery.exec() || !subquery.isActive())
                    MythContext::DBError("music playlist utf8 update", subquery);
            }
        }

        VERBOSE(VB_ALL, "Done updating music metadata to UTF-8");

        const QString updates[] = {
""
};
        performActualUpdate(updates, "1003", dbver);
    }
}

