#include <qsqldatabase.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythcontext.h"

const QString currentDatabaseVersion = "1005";

void UpdateDBVersionNumber(const QString &newnumber)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    db_conn->exec("DELETE FROM settings WHERE value='DBSchemaVer';");
    db_conn->exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('DBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

void InitializeDatabase(void)
{
    cerr << "Not written yet.\n";
    // set up all of the database tables.
}

void UpgradeTVDatabaseSchema(void)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    QString dbver = gContext->GetSetting("DBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        InitializeDatabase();        
        return;
    }


    // insert older upgrades here, later


    if (dbver == "1003")
    {
        VERBOSE(VB_ALL, "Upgrading to schema version 1004");
        UpdateDBVersionNumber("1004");
        dbver = "1004";    
    }

    if (dbver == "1004")
    {
        VERBOSE(VB_ALL, "Upgrading to schema version 1005");

        const QString updates[] = {
"DROP TABLE IF EXISTS profilegroups;",
"CREATE TABLE profilegroups ("
"  id int(10) unsigned NOT NULL auto_increment,"
"  name varchar(128) default NULL,"
"  cardtype varchar(32) NOT NULL default 'V4L',"
"  is_default int(1) default 0,"
"  hostname varchar(255) default NULL,"
"  PRIMARY KEY (id),"
"  UNIQUE (name, hostname)"
");",
"INSERT INTO profilegroups SET name = \"Software Encoders (v4l based)\", cardtype = 'V4L', is_default = 1;",
"INSERT INTO profilegroups SET name = \"MPEG-2 Encoders (PVR-250, PVR-350)\", cardtype = 'MPEG', is_default = 1;",
"INSERT INTO profilegroups SET name = \"Hardware MJPEG Encoders (Matrox G200-TV, Miro DC10, etc)\", cardtype = 'MJPEG', is_default = 1;",
"INSERT INTO profilegroups SET name = \"Hardware HDTV\", cardtype = 'HDTV', is_default = 1;",
"INSERT INTO profilegroups SET name = \"Hardware DVB Encoders\", cardtype = 'DVB', is_default = 1;",
"INSERT INTO profilegroups SET name = \"Transcoders\", cardtype = 'TRANSCODE', is_default = 1;",
"DROP TABLE recordingprofiles;",
"CREATE TABLE recordingprofiles ("
"  id int(10) unsigned NOT NULL auto_increment,"
"  name varchar(128) default NULL,"
"  videocodec varchar(128) default NULL,"
"  audiocodec varchar(128) default NULL,"
"  profilegroup int(10) unsigned NOT NULL DEFAULT 0,"
"  PRIMARY KEY (id),"
");",
"INSERT INTO recordingprofiles SET name = \"Default\", profilegroup = 1;",
"INSERT INTO recordingprofiles SET name = \"Live TV\", profilegroup = 1;",
"INSERT INTO recordingprofiles SET name = \"High Quality\", profilegroup = 1;",
"INSERT INTO recordingprofiles SET name = \"Low Quality\", profilegroup = 1;",
"INSERT INTO recordingprofiles SET name = \"Default\", profilegroup = 2;",
"INSERT INTO recordingprofiles SET name = \"Live TV\", profilegroup = 2;",
"INSERT INTO recordingprofiles SET name = \"High Quality\", profilegroup = 2;",
"INSERT INTO recordingprofiles SET name = \"Low Quality\", profilegroup = 2;",
"INSERT INTO recordingprofiles SET name = \"Default\", profilegroup = 3;",
"INSERT INTO recordingprofiles SET name = \"Live TV\", profilegroup = 3;",
"INSERT INTO recordingprofiles SET name = \"High Quality\", profilegroup = 3;",
"INSERT INTO recordingprofiles SET name = \"Low Quality\", profilegroup = 3;",
"INSERT INTO recordingprofiles SET name = \"Default\", profilegroup = 4;",
"INSERT INTO recordingprofiles SET name = \"Live TV\", profilegroup = 4;",
"INSERT INTO recordingprofiles SET name = \"High Quality\", profilegroup = 4;",
"INSERT INTO recordingprofiles SET name = \"Low Quality\", profilegroup = 4;",
"INSERT INTO recordingprofiles SET name = \"Default\", profilegroup = 5;",
"INSERT INTO recordingprofiles SET name = \"Live TV\", profilegroup = 5;",
"INSERT INTO recordingprofiles SET name = \"High Quality\", profilegroup = 5;",
"INSERT INTO recordingprofiles SET name = \"Low Quality\", profilegroup = 5;",
"INSERT INTO recordingprofiles SET name = \"RTjpeg/MPEG4\", profilegroup = 6;",
"INSERT INTO recordingprofiles SET name = \"MPEG2\", profilegroup = 6;",
"DELETE FROM codecparams;",
"",
};

        int counter = 0;
        QString thequery = updates[counter];
        while (thequery != "")
        {
            db_conn->exec(thequery);
            counter++;
            thequery = updates[counter];
        }

        UpdateDBVersionNumber("1005");
        dbver = "1005";
    }
}

