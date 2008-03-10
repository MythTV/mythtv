#include <qstring.h>
#include <qdir.h>
#include <qstringlist.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "dbcheck.h"
#include "defs.h"

const QString currentDatabaseVersion = "1001";

static void UpdateDBVersionNumber(const QString &newnumber)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.exec("DELETE FROM settings WHERE value='WeatherDBSchemaVer';");
    query.exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('WeatherDBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

static void performActualUpdate(const QStringList updates, QString version,
                                QString &dbver)
{
    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythWeather schema version ") +
            version);

    MSqlQuery query(MSqlQuery::InitCon());

    for (size_t i = 0; i < updates.size(); ++i)
    {
        if (!query.exec(updates[i]))
            VERBOSE(VB_IMPORTANT,
                    QObject::tr("ERROR Executing query %1").arg(updates[i]));
    }

    UpdateDBVersionNumber(version);
    dbver = version;
}

/*
 * TODO Probably the biggest change to simplify things would be to get rid of
 * the surrogate key screen_id in weatherscreens, draworder should be unique,
 * that way, with cascading, updating screens won't need to blow out everything
 * in the db everytime.
 */
void InitializeDatabase()
{
    QString dbver = gContext->GetSetting("WeatherDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT,
                "Inserting MythWeather initial database information.");
        QStringList updates;
        updates << "CREATE TABLE IF NOT EXISTS weathersourcesettings ("
                        "sourceid INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                        "source_name VARCHAR(64) NOT NULL,"
                        "update_timeout INT UNSIGNED NOT NULL DEFAULT '600',"
                        "retrieve_timeout INT UNSIGNED NOT NULL DEFAULT '60',"
                        "hostname VARCHAR(255) NULL,"
                        "path VARCHAR(255) NULL,"
                        "author VARCHAR(128) NULL,"
                        "version VARCHAR(32) NULL,"
                        "email VARCHAR(255) NULL,"
                        "types MEDIUMTEXT NULL,"
                        "PRIMARY KEY(sourceid)) TYPE=InnoDB;"
               << "CREATE TABLE IF NOT EXISTS weatherscreens ("
                        "screen_id INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                        "draworder INT UNSIGNED NOT NULL,"
                        "container VARCHAR(64) NOT NULL,"
                        "hostname VARCHAR(255) NULL,"
                        "units TINYINT UNSIGNED NOT NULL,"
                        "PRIMARY KEY(screen_id)) TYPE=InnoDB;"
               << "CREATE TABLE IF NOT EXISTS weatherdatalayout ("
                        "location VARCHAR(64) NOT NULL,"
                        "dataitem VARCHAR(64) NOT NULL,"
                        "weatherscreens_screen_id INT UNSIGNED NOT NULL,"
                        "weathersourcesettings_sourceid INT UNSIGNED NOT NULL,"
                        "PRIMARY KEY(location, dataitem, weatherscreens_screen_id,"
                            "weathersourcesettings_sourceid),"
                        "INDEX weatherdatalayout_FKIndex1(weatherscreens_screen_id),"
                        "INDEX weatherdatalayout_FKIndex2(weathersourcesettings_sourceid),"
                        "FOREIGN KEY(weatherscreens_screen_id) "
                        "REFERENCES weatherscreens(screen_id) "
                            "ON DELETE CASCADE "
                            "ON UPDATE CASCADE,"
                        "FOREIGN KEY(weathersourcesettings_sourceid) "
                        "REFERENCES weathersourcesettings(sourceid) "
                        "ON DELETE RESTRICT "
                        "ON UPDATE CASCADE) TYPE=InnoDB;";
        /*
         * TODO Possible want to delete old stuff (i.e. agressiveness, locale..)
         * that we don't use any more
         */

        performActualUpdate(updates, "1000", dbver);
    }

    if (dbver == "1000")
    {
        QStringList updates;
        updates << "ALTER TABLE weathersourcesettings ADD COLUMN updated "
                   "TIMESTAMP;";

        performActualUpdate(updates, "1001", dbver);
    }
}
