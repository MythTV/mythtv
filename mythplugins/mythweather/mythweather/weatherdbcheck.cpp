// Qt
#include <QDir>
#include <QSqlError>
#include <QString>
#include <QStringList>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdbcheck.h>

// MythWeather
#include "weatherdbcheck.h"

const QString currentDatabaseVersion = "1007";
const QString MythWeatherVersionName = "WeatherDBSchemaVer";

/*
 * TODO Probably the biggest change to simplify things would be to get rid of
 * the surrogate key screen_id in weatherscreens, draworder should be unique,
 * that way, with cascading, updating screens won't need to blow out everything
 * in the db everytime.
 */
bool InitializeDatabase()
{
    QString dbver = gCoreContext->GetSetting("WeatherDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "Inserting MythWeather initial database information.");
        DBUpdates updates {
                   "CREATE TABLE IF NOT EXISTS weathersourcesettings ("
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
                        "PRIMARY KEY(sourceid)) ENGINE=InnoDB;",
                  "CREATE TABLE IF NOT EXISTS weatherscreens ("
                        "screen_id INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                        "draworder INT UNSIGNED NOT NULL,"
                        "container VARCHAR(64) NOT NULL,"
                        "hostname VARCHAR(255) NULL,"
                        "units TINYINT UNSIGNED NOT NULL,"
                        "PRIMARY KEY(screen_id)) ENGINE=InnoDB;",
                  "CREATE TABLE IF NOT EXISTS weatherdatalayout ("
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
                        "ON UPDATE CASCADE) ENGINE=InnoDB;"
        };
        /*
         * TODO Possible want to delete old stuff (i.e. agressiveness, locale..)
         * that we don't use any more
         */

        if (!performActualUpdate("MythWeather", MythWeatherVersionName,
                                 updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        DBUpdates updates {
                   "ALTER TABLE weathersourcesettings ADD COLUMN updated "
                   "TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP "
                   "          ON UPDATE CURRENT_TIMESTAMP;"
        };

        if (!performActualUpdate("MythWeather", MythWeatherVersionName,
                                 updates, "1001", dbver))
            return false;
    }



    if (dbver == "1001")
    {
        DBUpdates updates {
            qPrintable(QString("ALTER DATABASE %1 DEFAULT CHARACTER SET latin1;")
            .arg(GetMythDB()->GetDatabaseName())),
            "ALTER TABLE weatherdatalayout"
            "  MODIFY location varbinary(64) NOT NULL,"
            "  MODIFY dataitem varbinary(64) NOT NULL;",
            "ALTER TABLE weatherscreens"
            "  MODIFY container varbinary(64) NOT NULL,"
            "  MODIFY hostname varbinary(64) default NULL;",
            "ALTER TABLE weathersourcesettings"
            "  MODIFY source_name varbinary(64) NOT NULL,"
            "  MODIFY hostname varbinary(64) default NULL,"
            "  MODIFY path varbinary(255) default NULL,"
            "  MODIFY author varbinary(128) default NULL,"
            "  MODIFY version varbinary(32) default NULL,"
            "  MODIFY email varbinary(255) default NULL,"
            "  MODIFY types mediumblob;"
        };

        if (!performActualUpdate("MythWeather", MythWeatherVersionName,
                                 updates, "1002", dbver))
            return false;
    }


    if (dbver == "1002")
    {
        DBUpdates updates {
            qPrintable(QString("ALTER DATABASE %1 DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;")
            .arg(GetMythDB()->GetDatabaseName())),
            "ALTER TABLE weatherdatalayout"
            "  DEFAULT CHARACTER SET utf8,"
            "  MODIFY location varchar(64) CHARACTER SET utf8 NOT NULL,"
            "  MODIFY dataitem varchar(64) CHARACTER SET utf8 NOT NULL;",
            "ALTER TABLE weatherscreens"
            "  DEFAULT CHARACTER SET utf8,"
            "  MODIFY container varchar(64) CHARACTER SET utf8 NOT NULL,"
            "  MODIFY hostname varchar(64) CHARACTER SET utf8 default NULL;",
            "ALTER TABLE weathersourcesettings"
            "  DEFAULT CHARACTER SET utf8,"
            "  MODIFY source_name varchar(64) CHARACTER SET utf8 NOT NULL,"
            "  MODIFY hostname varchar(64) CHARACTER SET utf8 default NULL,"
            "  MODIFY path varchar(255) CHARACTER SET utf8 default NULL,"
            "  MODIFY author varchar(128) CHARACTER SET utf8 default NULL,"
            "  MODIFY version varchar(32) CHARACTER SET utf8 default NULL,"
            "  MODIFY email varchar(255) CHARACTER SET utf8 default NULL,"
            "  MODIFY types mediumtext CHARACTER SET utf8;"
        };

        if (!performActualUpdate("MythWeather", MythWeatherVersionName,
                                 updates, "1003", dbver))
            return false;
    }

    if (dbver == "1003")
    {
        DBUpdates updates {
            "DELETE FROM keybindings "
            " WHERE action = 'DELETE' AND context = 'Weather';"
        };

        if (!performActualUpdate("MythWeather", MythWeatherVersionName,
                                 updates, "1004", dbver))
            return false;
    }

    if (dbver == "1004")
    {
        DBUpdates updates {
            "ALTER TABLE weatherdatalayout"
            "  MODIFY location varchar(128) CHARACTER SET utf8 NOT NULL;"
        };

        if (!performActualUpdate("MythWeather", MythWeatherVersionName,
                                 updates, "1005", dbver))
            return false;
    }

    if (dbver == "1005")
    {
        DBUpdates updates {
            "ALTER TABLE weathersourcesettings MODIFY COLUMN updated "
            "  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP "
            "            ON UPDATE CURRENT_TIMESTAMP;"
        };

        if (!performActualUpdate("MythWeather", MythWeatherVersionName,
                                 updates, "1006", dbver))
            return false;
    }

    // Repeat 1002 DBs pre MySQL v8 systems that may have not be set to utf8

    if (dbver == "1006")
    {
        DBUpdates updates {
            "ALTER TABLE weatherdatalayout DEFAULT CHARACTER SET utf8;"
            "ALTER TABLE weatherscreens DEFAULT CHARACTER SET utf8;"
            "ALTER TABLE weathersourcesettings DEFAULT CHARACTER SET utf8;"
        };

        if (!performActualUpdate("MythWeather", MythWeatherVersionName,
                                 updates, "1007", dbver))
            return false;
    }
    return true;
}
