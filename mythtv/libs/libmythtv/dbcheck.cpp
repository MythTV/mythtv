#include <qsqldatabase.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythcontext.h"

const QString currentDatabaseVersion = "1026";

void UpdateDBVersionNumber(const QString &newnumber)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    db_conn->exec("DELETE FROM settings WHERE value='DBSchemaVer';");
    db_conn->exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('DBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

void performActualUpdate(const QString updates[], QString version,
                         QString &dbver)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    VERBOSE(VB_ALL, QString("Upgrading to schema version ") + version);

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

void InitializeDatabase(void);

void UpgradeTVDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("DBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        InitializeDatabase();        
        dbver = "1003";
    }

    if (dbver == "900")
    {
        const QString updates[] = {
"ALTER TABLE program ADD COLUMN category_type VARCHAR(64) NULL;",
"DROP TABLE IF EXISTS transcoding;",
"CREATE TABLE transcoding (chanid INT UNSIGNED, starttime TIMESTAMP, status INT, hostname VARCHAR(255));",
""
};
        performActualUpdate(updates, "901", dbver);
    }

    if (dbver == "901")
    {
        const QString updates[] = {
"ALTER TABLE record ADD COLUMN category VARCHAR(64) NULL;",
"ALTER TABLE recorded ADD COLUMN category VARCHAR(64) NULL;",
"ALTER TABLE oldrecorded ADD COLUMN category VARCHAR(64) NULL;",
""
};
        performActualUpdate(updates, "902", dbver);
    }

    if (dbver == "902")
    {
        const QString updates[] = {
"ALTER TABLE record ADD rank INT(10) DEFAULT '0' NOT NULL;",
"ALTER TABLE channel ADD rank INT(10) DEFAULT '0' NOT NULL;",
""
};
        performActualUpdate(updates, "903", dbver);
    }

    if (dbver == "903")
    {
        const QString updates[] = {
"ALTER TABLE channel ADD COLUMN freqid VARCHAR(5) NOT NULL;",
"UPDATE channel set freqid=channum;",
"ALTER TABLE channel ADD hue INT DEFAULT '32768';",
""
};
        performActualUpdate(updates, "1000", dbver);
    }

    if (dbver == "1000")
    {
        const QString updates[] = {
"ALTER TABLE record ADD autoexpire INT DEFAULT 0 NOT NULL;",
"ALTER TABLE recorded ADD autoexpire INT DEFAULT 0 NOT NULL;",
""
};
        performActualUpdate(updates, "1001", dbver);
    }

    if (dbver == "1001")
    {
        const QString updates[] = {
"ALTER TABLE record ADD maxepisodes INT DEFAULT 0 NOT NULL;",
""
};
        performActualUpdate(updates, "1002", dbver);
    }

    if (dbver == "1002")
    {
        const QString updates[] = {
"ALTER TABLE record ADD recorddups INT DEFAULT 0 NOT NULL;",
"ALTER TABLE record ADD maxnewest INT DEFAULT 0 NOT NULL;",
""
};
        performActualUpdate(updates, "1003", dbver);
    }

    if (dbver == "1003")
    {
        const QString updates[] = {
""
};
        performActualUpdate(updates, "1004", dbver);
    }

    if (dbver == "1004")
    {
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
"  PRIMARY KEY (id)"
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
""
};
        performActualUpdate(updates, "1005", dbver);
    }

    if (dbver == "1005")
    {
        const QString updates[] = {
"DELETE FROM settings where value = 'TranscoderAutoRun';",
"ALTER TABLE record CHANGE profile profile VARCHAR(128);",
""
};
        performActualUpdate(updates, "1006", dbver);
    }

    if (dbver == "1006")
    {
        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS recordingprofiles ("
"  id int(10) unsigned NOT NULL auto_increment,"
"  name varchar(128) default NULL,"
"  videocodec varchar(128) default NULL,"
"  audiocodec varchar(128) default NULL,"
"  profilegroup int(10) unsigned NOT NULL DEFAULT 0,"
"  PRIMARY KEY (id)"
");",
""
};
        performActualUpdate(updates, "1007", dbver);
    }

    if (dbver == "1007")
    {
        const QString updates[] = {
"ALTER TABLE capturecard CHANGE use_ts dvb_swfilter INT DEFAULT '0';",
"ALTER TABLE capturecard CHANGE dvb_type dvb_recordts INT DEFAULT '0';",
"DROP TABLE IF EXISTS channel_dvb;",
"CREATE TABLE IF NOT EXISTS dvb_channel ("
"   chanid              SMALLINT NOT NULL,"
"   serviceid           SMALLINT NULL,"
"   networkid           SMALLINT NULL,"
"   providerid          SMALLINT NULL,"
"   transportid         SMALLINT NULL,"
"   frequency           INTEGER NULL,"
"   inversion           CHAR(1) NULL,"
"   symbolrate          INTEGER NULL,"
"   fec                 VARCHAR(10) NULL,"
"   polarity            CHAR(1) NULL,"
"   satid               SMALLINT NULL,"
"   modulation          VARCHAR(10) NULL,"
"   bandwidth           CHAR(1) NULL,"
"   lp_code_rate        VARCHAR(10) NULL,"
"   transmission_mode   CHAR(1) NULL,"
"   guard_interval      VARCHAR(10) NULL,"
"   hierarchy           CHAR(1) NULL,"
"   PRIMARY KEY (chanid)"
");",
"CREATE TABLE IF NOT EXISTS dvb_sat ("
"   satid   SMALLINT NOT NULL AUTO_INCREMENT,"
"   lnbid   SMALLINT NOT NULL,"
"   cardnum SMALLINT NOT NULL,"
"   PRIMARY KEY (cardnum,satid,lnbid)"
");",
"CREATE TABLE IF NOT EXISTS dvb_lnb ("
"   lnbid       SMALLINT NOT NULL AUTO_INCREMENT,"
"   disecqid    SMALLINT NULL,"
"   diseqc_port SMALLINT NULL,"
"   lof_switch  INTEGER DEFAULT 11700000 NOT NULL,"
"   lof_hi      INTEGER DEFAULT 10600000 NOT NULL,"
"   lof_lo      INTEGER DEFAULT 9750000 NOT NULL,"
"   PRIMARY KEY (lnbid)"
");",
"CREATE TABLE IF NOT EXISTS dvb_pids ("
"   chanid  SMALLINT NOT NULL,"
"   pid     SMALLINT NOT NULL,"
"   type    CHAR(1) DEFAULT 'o' NOT NULL,"
"   lang    CHAR(3) DEFAULT '' NOT NULL,"
"   PRIMARY KEY (chanid,pid)"
");",
""
};
        performActualUpdate(updates, "1008", dbver);
    }

    if (dbver == "1008")
    {
        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS dvb_sat ("
"   satid   SMALLINT NOT NULL AUTO_INCREMENT,"
"   lnbid   SMALLINT NOT NULL,"
"   cardnum SMALLINT NOT NULL,"
"   PRIMARY KEY (cardnum,satid,lnbid)"
");",
""
};
        performActualUpdate(updates, "1009", dbver);
    }

    if (dbver == "1009")
    {
        const QString updates[] = {
"ALTER TABLE record ADD COLUMN preroll INT DEFAULT 0 NOT NULL;",
"ALTER TABLE record ADD COLUMN postroll INT DEFAULT 0 NOT NULL;",
""
};
        performActualUpdate(updates, "1010", dbver);
    }

    if (dbver == "1010")
    {
        const QString updates[] = {
"DROP TABLE IF EXISTS dvb_diseqc;",
"DROP TABLE IF EXISTS dvb_lnb;",
"DROP TABLE IF EXISTS dvb_sat;",
"CREATE TABLE IF NOT EXISTS dvb_sat ("
"   satid SMALLINT NOT NULL AUTO_INCREMENT,"
"   sourceid INT DEFAULT 0,"
"   pos SMALLINT DEFAULT 0,"
"   name VARCHAR(128),"
"   diseqc_type SMALLINT DEFAULT 0,"
"   diseqc_port SMALLINT DEFAULT 0,"
"   lnb_lof_switch SMALLINT DEFAULT 11700000,"
"   lnb_lof_hi INTEGER DEFAULT 10600000,"
"   lnb_lof_lo INTEGER DEFAULT 9750000,"
"   PRIMARY KEY(satid)"
");",
""
};
        performActualUpdate(updates, "1011", dbver);
    }

    if (dbver == "1011")
    {
        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS recordoverride ("
"   recordid INT UNSIGNED NOT NULL, "
"   type INT UNSIGNED NOT NULL, "
"   chanid INT UNSIGNED NULL, "
"   starttime TIMESTAMP NOT NULL, "
"   endtime TIMESTAMP NOT NULL, "
"   title VARCHAR(128) NULL, "
"   subtitle VARCHAR(128) NULL, "
"   description TEXT NULL "
");",
""
};
        performActualUpdate(updates, "1012", dbver);
    }

    if (dbver == "1012")
    {
        const QString updates[] = {
"INSERT INTO settings SET value=\"mythfilldatabaseLastRunStart\";",
"INSERT INTO settings SET value=\"mythfilldatabaseLastRunEnd\";",
"INSERT INTO settings SET value=\"mythfilldatabaseLastRunStatus\";",
""
};
        performActualUpdate(updates, "1013", dbver);
    }

    if (dbver == "1013")
    {
        const QString updates[] = {
"ALTER TABLE record CHANGE rank recpriority INT(10) DEFAULT '0' NOT NULL;",
"ALTER TABLE channel CHANGE rank recpriority INT(10) DEFAULT '0' NOT NULL;",
"UPDATE settings SET value='RecPriorityingActive' WHERE value='RankingActive';",
"UPDATE settings SET value='RecPriorityingOrder' WHERE value='RankingOrder';",
"UPDATE settings SET value='SingleRecordRecPriority' WHERE value='SingleRecordRank';",
"UPDATE settings SET value='TimeslotRecordRecPriority' WHERE value='TimeslotRecordRank';",
"UPDATE settings SET value='WeekslotRecordRecPriority' WHERE value='WeekslotRecordRank';",
"UPDATE settings SET value='ChannelRecordRecPriority' WHERE value='ChannelRecordRank';",
"UPDATE settings SET value='AllRecordRecPriority' WHERE value='AllRecordRank';",
"UPDATE settings SET value='ChannelRecPrioritySorting' WHERE value='ChannelRankSorting';",
"UPDATE settings SET value='ProgramRecPrioritySorting' WHERE value='ProgramRankSorting';",
""
};
        performActualUpdate(updates, "1014", dbver);
    }

    if (dbver == "1014")
    {
        const QString updates[] = {
"ALTER TABLE dvb_sat CHANGE lnb_log_switch lnb_lof_switch INTEGER DEFAULT 11700000;",
"UPDATE settings SET value='RecPriorityOrder' WHERE value='RecPriorityingOrder';",
""
};

        performActualUpdate(updates, "1015", dbver);
    }

    if (dbver == "1015")
    {
        const QString updates[] = {
"CREATE TABLE jumppoints ("
"  destination varchar(128) NOT NULL,"
"  description varchar(255) default NULL,"
"  keylist varchar(32) default NULL,"
"  hostname varchar(255) NOT NULL"
");",
"CREATE TABLE keybindings ("
"  context varchar(32) NOT NULL,"
"  action varchar(32) NOT NULL,"
"  description varchar(255) default NULL,"
"  keylist varchar(32) default NULL,"
"  hostname varchar(255) NOT NULL"
");",
""
};

        performActualUpdate(updates, "1016", dbver);
    }

    if (dbver == "1016")
    {
        const QString updates[] = {
"ALTER TABLE jumppoints ADD PRIMARY KEY (destination, hostname);",
"ALTER TABLE keybindings ADD PRIMARY KEY (context, action, hostname);",
""
};

        performActualUpdate(updates, "1017", dbver);
    }

    if (dbver == "1017")
    {
        const QString updates[] = {
"UPDATE settings SET value='RecPriorityActive' WHERE value='RecPriorityingActive';",
""
};

        performActualUpdate(updates, "1018", dbver);
    }

    if (dbver == "1018")
    {
        const QString updates[] = {
"ALTER TABLE channel ADD COLUMN tvformat VARCHAR(10) NOT NULL default 'Default';",
""
};

        performActualUpdate(updates, "1019", dbver);
    }

    if (dbver == "1019")
    {
        const QString updates[] = {
"UPDATE channel set tvformat = 'Default' where tvformat = '';",
"DELETE FROM keybindings WHERE context = 'TV Playback' and action = 'CONTRASTDOWN';",
"DELETE FROM keybindings WHERE context = 'TV Playback' and action = 'CONTRASTUP';",
"DELETE FROM keybindings WHERE context = 'TV Playback' and action = 'BRIGHTDOWN';",
"DELETE FROM keybindings WHERE context = 'TV Playback' and action = 'BRIGHTUP';",
"DELETE FROM keybindings WHERE context = 'TV Playback' and action = 'COLORDOWN';",
"DELETE FROM keybindings WHERE context = 'TV Playback' and action = 'COLORUP';",
"DELETE FROM keybindings WHERE context = 'TV Playback' and action = 'HUEDOWN';",
"DELETE FROM keybindings WHERE context = 'TV Playback' and action = 'HUEUP';",
""
};

        performActualUpdate(updates, "1020", dbver);
    }

    if (dbver == "1020")
    {
        const QString updates[] = {
"CREATE TABLE oldprogram ("
"  oldtitle VARCHAR(128) NOT NULL PRIMARY KEY,"
"  airdate TIMESTAMP NOT NULL"
");",
""
};

        performActualUpdate(updates, "1021", dbver);
    }

    if (dbver == "1021")
    {
        const QString updates[] = {
"ALTER TABLE recorded ADD COLUMN commflagged int(10) unsigned NOT NULL default '0';",
""
};

        performActualUpdate(updates, "1022", dbver);

        VERBOSE(VB_ALL, QString("This could take a few minutes."));

        QSqlDatabase *db = QSqlDatabase::database();

        QString thequery, chanid, startts;
        QSqlQuery query;
        QDateTime recstartts;

        thequery = QString("SELECT recorded.chanid, recorded.starttime "
                           "FROM recorded, recordedmarkup "
                           "WHERE recorded.chanid = recordedmarkup.chanid AND "
                           "recorded.starttime = recordedmarkup.starttime AND "
                           "(type = 4 OR type = 5) "
                           "GROUP BY chanid, starttime");
        query = db->exec(thequery);

        vector<QString*> cfList;

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                chanid  = query.value(0).toString();
                recstartts = query.value(1).toDateTime();
                startts = recstartts.toString("yyyyMMddhhmm");
                startts += "00";

                QString *tmp = new QString[2];
                tmp[0] = chanid;
                tmp[1] = startts;
                cfList.push_back(tmp);
            }
        }

        for (unsigned int i = 0; i < cfList.size(); i++)
        {
            chanid = cfList[i][0];
            startts = cfList[i][1];

            thequery = QString("UPDATE recorded SET commflagged = 1, "
                               "starttime = %2 WHERE chanid = '%1' AND "
                               "starttime = '%3';").arg(chanid).arg(startts)
                                                   .arg(startts);

            db->exec(thequery);
            delete cfList[i];
        }
    }

    if (dbver == "1022")
    {
        const QString updates[] = {
"ALTER TABLE dvb_sat CHANGE lnb_lof_switch lnb_lof_switch INTEGER DEFAULT 11700000;",
""
};
        performActualUpdate(updates, "1023", dbver);
    }

    if (dbver == "1023")
    {
        const QString updates[] = {
QString("ALTER TABLE videosource ADD COLUMN freqtable VARCHAR(16) NOT NULL DEFAULT 'default';"),
""
};
        performActualUpdate(updates, "1024", dbver);
    }

    if (dbver == "1024")
    {
        const QString updates[] = {
"ALTER TABLE dvb_sat CHANGE sourceid cardid INT;",
"ALTER TABLE capturecard ADD COLUMN dvb_sat_type INT NOT NULL DEFAULT 0;",
"ALTER TABLE dvb_channel ADD COLUMN pmtcache BLOB;",
""
};
        performActualUpdate(updates, "1025", dbver);
    }

    if (dbver == "1025")
    {
        const QString updates[] = {
"CREATE TABLE keyword ("
"  phrase VARCHAR(128) NOT NULL PRIMARY KEY,"
"  UNIQUE(phrase)"
");",
""
};

        performActualUpdate(updates, "1026", dbver);
    }
}

void InitializeDatabase(void)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();
    QSqlQuery qQuery = db_conn->exec("SHOW TABLES;");
    if (qQuery.isActive() && (qQuery.numRowsAffected() > 0))
    {
        cerr << "Told to create a NEW database schema, but the database already"
             << "\r\nhas " << qQuery.numRowsAffected() << " tables.\r\n";
        cerr << "If you are sure this is a good mythtv database, verify\r\n"
             << "that the settings table has the DBSchemaVer variable.\r\n";
        exit(1);
    }

    VERBOSE(VB_ALL, "Inserting MythTV initial database information.");

    const QString updates[] = {
"CREATE TABLE IF NOT EXISTS recordingprofiles"
"("
"    id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    name VARCHAR(128),"
"    videocodec VARCHAR(128),"
"    audiocodec VARCHAR(128),"
"    UNIQUE(name)"
");",
"CREATE TABLE IF NOT EXISTS codecparams"
"("
"    profile INT UNSIGNED NOT NULL,"
"    name VARCHAR(128) NOT NULL,"
"    value VARCHAR(128),"
"    PRIMARY KEY (profile, name)"
");",
"CREATE TABLE IF NOT EXISTS channel"
"("
"    chanid INT UNSIGNED NOT NULL PRIMARY KEY,"
"    channum VARCHAR(5) NOT NULL,"
"    freqid VARCHAR(5) NOT NULL,"
"    sourceid INT UNSIGNED,"
"    callsign VARCHAR(20) NULL,"
"    name VARCHAR(20) NULL,"
"    icon VARCHAR(255) NULL,"
"    finetune INT,"
"    videofilters VARCHAR(255) NULL,"
"    xmltvid VARCHAR(64) NULL,"
"    rank INT(10) DEFAULT '0' NOT NULL,"
"    contrast INT DEFAULT 32768,"
"    brightness INT DEFAULT 32768,"
"    colour INT DEFAULT 32768,"
"    hue INT DEFAULT 32768"
");",
"CREATE TABLE IF NOT EXISTS channel_dvb"
"("
"    chanid INT UNSIGNED NOT NULL PRIMARY KEY,"
"    listingid VARCHAR(20) NULL,"
"    pids VARCHAR(50),"
"    freq INT UNSIGNED,"
"    pol CHAR DEFAULT 'V',"
"    symbol_rate INT UNSIGNED NULL,"
"    tone INT UNSIGNED NULL,"
"    diseqc INT UNSIGNED NULL,"
"    inversion VARCHAR(10) NULL,"
"    bandwidth VARCHAR(10) NULL,"
"    hp_code_rate VARCHAR(10) NULL,"
"    lp_code_rate VARCHAR(10) NULL,"
"    modulation VARCHAR(10) NULL,"
"    transmission_mode VARCHAR(10) NULL,"
"    guard_interval VARCHAR(10) NULL,"
"    hierarchy VARCHAR(10) NULL"
");",
"CREATE TABLE IF NOT EXISTS program"
"("
"    chanid INT UNSIGNED NOT NULL,"
"    starttime TIMESTAMP NOT NULL,"
"    endtime TIMESTAMP NOT NULL,"
"    title VARCHAR(128) NULL,"
"    subtitle VARCHAR(128) NULL,"
"    description TEXT NULL,"
"    category VARCHAR(64) NULL,"
"    category_type VARCHAR(64) NULL,"
"    airdate YEAR NOT NULL,"
"    stars FLOAT UNSIGNED NOT NULL,"
"    previouslyshown TINYINT NOT NULL default '0',"
"    PRIMARY KEY (chanid, starttime),"
"    INDEX (endtime),"
"    INDEX (title)"
");",
"CREATE TABLE IF NOT EXISTS record"
"("
"    recordid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    type INT UNSIGNED NOT NULL,"
"    chanid INT UNSIGNED NULL,"
"    starttime TIME NOT NULL,"
"    startdate DATE NOT NULL,"
"    endtime TIME NOT NULL,"
"    enddate DATE NOT NULL,"
"    title VARCHAR(128) NULL,"
"    subtitle VARCHAR(128) NULL,"
"    description TEXT NULL,"
"    category VARCHAR(64) NULL,"
"    profile INT UNSIGNED NOT NULL DEFAULT 0,"
"    rank INT(10) DEFAULT '0' NOT NULL,"
"    autoexpire INT DEFAULT 0 NOT NULL,"
"    maxepisodes INT DEFAULT 0 NOT NULL,"
"    maxnewest INT DEFAULT 0 NOT NULL,"
"    recorddups INT DEFAULT 0 NOT NULL,"
"    INDEX (chanid, starttime),"
"    INDEX (title)"
");",
"CREATE TABLE IF NOT EXISTS recorded"
"("
"    chanid INT UNSIGNED NOT NULL,"
"    starttime TIMESTAMP NOT NULL,"
"    endtime TIMESTAMP NOT NULL,"
"    title VARCHAR(128) NULL,"
"    subtitle VARCHAR(128) NULL,"
"    description TEXT NULL,"
"    category VARCHAR(64) NULL,"
"    hostname VARCHAR(255),"
"    bookmark VARCHAR(128) NULL,"
"    editing INT UNSIGNED NOT NULL DEFAULT 0,"
"    cutlist TEXT NULL,"
"    autoexpire INT DEFAULT 0 NOT NULL,"
"    PRIMARY KEY (chanid, starttime),"
"    INDEX (endtime)"
");",
"CREATE TABLE IF NOT EXISTS settings"
"("
"    value VARCHAR(128) NOT NULL,"
"    data TEXT NULL,"
"    hostname VARCHAR(255) NULL,"
"    INDEX (value, hostname)"
");",
"CREATE TABLE IF NOT EXISTS conflictresolutionoverride"
"("
"    chanid INT UNSIGNED NOT NULL,"
"    starttime TIMESTAMP NOT NULL,"
"    endtime TIMESTAMP NOT NULL,"
"    INDEX (chanid, starttime),"
"    INDEX (endtime)"
");",
"CREATE TABLE IF NOT EXISTS conflictresolutionsingle"
"("
"    preferchanid INT UNSIGNED NOT NULL,"
"    preferstarttime TIMESTAMP NOT NULL,"
"    preferendtime TIMESTAMP NOT NULL,"
"    dislikechanid INT UNSIGNED NOT NULL,"
"    dislikestarttime TIMESTAMP NOT NULL,"
"    dislikeendtime TIMESTAMP NOT NULL,"
"    INDEX (preferchanid, preferstarttime),"
"    INDEX (preferendtime)"
");",
"CREATE TABLE IF NOT EXISTS conflictresolutionany"
"("
"    prefertitle VARCHAR(128) NOT NULL,"
"    disliketitle VARCHAR(128) NOT NULL,"
"    INDEX (prefertitle),"
"    INDEX (disliketitle)"
");",
"CREATE TABLE IF NOT EXISTS oldrecorded"
"("
"    chanid INT UNSIGNED NOT NULL,"
"    starttime TIMESTAMP NOT NULL,"
"    endtime TIMESTAMP NOT NULL,"
"    title VARCHAR(128) NULL,"
"    subtitle VARCHAR(128) NULL,"
"    description TEXT NULL,"
"    category VARCHAR(64) NULL,"
"    PRIMARY KEY (chanid, starttime),"
"    INDEX (endtime),"
"    INDEX (title)"
");",
"CREATE TABLE IF NOT EXISTS capturecard"
"("
"    cardid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    videodevice VARCHAR(128),"
"    audiodevice VARCHAR(128),"
"    vbidevice VARCHAR(128),"
"    cardtype VARCHAR(32) DEFAULT 'V4L',"
"    defaultinput VARCHAR(32) DEFAULT 'Television',"
"    audioratelimit INT,"
"    hostname VARCHAR(255),"
"    use_ts INT NULL,"
"    dvb_type CHAR NULL"
");",
"CREATE TABLE IF NOT EXISTS videosource"
"("
"    sourceid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    name VARCHAR(128) NOT NULL,"
"    xmltvgrabber VARCHAR(128),"
"    userid VARCHAR(128) NOT NULL DEFAULT '',"
"    UNIQUE(name)"
");",
"CREATE TABLE IF NOT EXISTS cardinput"
"("
"    cardinputid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    cardid INT UNSIGNED NOT NULL,"
"    sourceid INT UNSIGNED NOT NULL,"
"    inputname VARCHAR(32) NOT NULL,"
"    externalcommand VARCHAR(128) NULL,"
"    preference INT,"
"    shareable CHAR DEFAULT 'N',"
"    tunechan CHAR(5) NOT NULL,"
"    startchan CHAR(5) NOT NULL"
");",
"CREATE TABLE IF NOT EXISTS favorites ("
"    favid int(11) unsigned NOT NULL auto_increment,"
"    userid int(11) unsigned NOT NULL default '0',"
"    chanid int(11) unsigned NOT NULL default '0',"
"    PRIMARY KEY (favid)"
");",
"CREATE TABLE IF NOT EXISTS recordedmarkup"
"("
"    chanid INT UNSIGNED NOT NULL,"
"    starttime TIMESTAMP NOT NULL,"
"    mark BIGINT(20) NOT NULL,"
"    offset VARCHAR(32) NULL,"
"    type INT NOT NULL,"
"    primary key (chanid,starttime, mark, type )"
");",
"CREATE TABLE IF NOT EXISTS programrating"
"("
"    chanid INT UNSIGNED NOT NULL,"
"    starttime TIMESTAMP NOT NULL,"
"    system CHAR(8) NOT NULL default '',"
"    rating CHAR(8) NOT NULL default '',"
"    UNIQUE KEY chanid (chanid,starttime,system,rating),"
"    INDEX (starttime, system)"
");",
"CREATE TABLE IF NOT EXISTS people"
"("
"    person MEDIUMINT(8) UNSIGNED NOT NULL AUTO_INCREMENT,"
"    name CHAR(128) NOT NULL default '',"
"    PRIMARY KEY (person),"
"    KEY name (name(20))"
") TYPE=MyISAM;",
"CREATE TABLE IF NOT EXISTS credits"
"("
"    person MEDIUMINT(8) UNSIGNED NOT NULL default '0',"
"    chanid INT UNSIGNED NOT NULL default '0',"
"    starttime TIMESTAMP NOT NULL,"
"    role SET('actor','director','producer','executive_producer','writer','guest_star','host','adapter','presenter','commentator','guest') NOT NULL default '',"
"    UNIQUE KEY chanid (chanid, starttime, person),"
"    KEY person (person, role)"
") TYPE=MyISAM;",
"CREATE TABLE IF NOT EXISTS transcoding ("
"    chanid INT UNSIGNED,"
"    starttime TIMESTAMP,"
"    status INT,"
"    hostname VARCHAR(255)"
");",
"INSERT INTO settings VALUES ('DBSchemaVer', 1003, NULL);",
"INSERT INTO recordingprofiles (name) VALUES ('Default');",
"INSERT INTO recordingprofiles (name) VALUES ('Live TV');",
"INSERT INTO recordingprofiles (name) VALUES ('Transcode');",
""
};

    QString dbver = "";
    performActualUpdate(updates, "1003", dbver);
}

