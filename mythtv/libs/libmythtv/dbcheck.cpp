#include <qsqldatabase.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythcontext.h"

const QString currentDatabaseVersion = "1055";

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

        if (db_conn->lastError().type() != QSqlError::None)
        {
            cerr << "DB Error (Performing database upgrade):" << endl;
            cerr << "Query was:" << endl
                 << thequery << endl
                 << MythContext::DBErrorMessage(db_conn->lastError()) << endl;

            exit(-13);
        }

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
"ALTER TABLE dvb_sat CHANGE lnb_lof_switch lnb_lof_switch INTEGER DEFAULT 11700000;",
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

        thequery = QString("SELECT distinct recorded.chanid, "
                           "recorded.starttime "
                           "FROM recorded, recordedmarkup "
                           "WHERE recorded.chanid = recordedmarkup.chanid AND "
                           "recorded.starttime = recordedmarkup.starttime AND "
                           "(type = 4 OR type = 5) ");
        query = db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                chanid  = query.value(0).toString();
                recstartts = query.value(1).toDateTime();
                startts = recstartts.toString("yyyyMMddhhmm");
                startts += "00";

                thequery = QString("UPDATE recorded SET commflagged = 1, "
                                   "starttime = %2 WHERE chanid = '%1' AND "
                                   "starttime = '%3';").arg(chanid).arg(startts)
                                                       .arg(startts);

                db->exec(thequery);
            }  
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

    if (dbver == "1026")
    {
        const QString updates[] = {
"ALTER TABLE capturecard ADD COLUMN dvb_wait_for_seqstart INT NOT NULL DEFAULT 1;",
"ALTER TABLE capturecard ADD COLUMN dvb_dmx_buf_size INT NOT NULL DEFAULT 8192;",
"ALTER TABLE capturecard ADD COLUMN dvb_pkt_buf_size INT NOT NULL DEFAULT 8192;",
""
};
        performActualUpdate(updates, "1027", dbver);
    }

    if (dbver == "1027")
    {
        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS dvb_signal_quality ("
"    id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    sampletime TIMESTAMP NOT NULL,"
"    cardid INT UNSIGNED NOT NULL,"
"    fe_snr INT UNSIGNED NOT NULL,"
"    fe_ss  INT UNSIGNED NOT NULL,"
"    fe_ber INT UNSIGNED NOT NULL,"
"    fe_unc INT UNSIGNED NOT NULL,"
"    myth_cont INT UNSIGNED NOT NULL,"
"    myth_over INT UNSIGNED NOT NULL,"
"    myth_pkts INT UNSIGNED NOT NULL,"
"    FOREIGN KEY(cardid) REFERENCES capturecard(id),"
"    INDEX (sampletime,cardid)"
");",
"ALTER TABLE capturecard ADD skipbtaudio TINYINT(1) DEFAULT 0;",
""
};
        performActualUpdate(updates, "1028", dbver);
    }

    if (dbver == "1028") {
        const QString updates[] = {
"ALTER TABLE channel ADD COLUMN commfree TINYINT NOT NULL default '0';",
"ALTER TABLE record ADD COLUMN recgroup VARCHAR(32) default 'Default';",
"ALTER TABLE record ADD COLUMN dupmethod INT NOT NULL DEFAULT 6;",
"ALTER TABLE record ADD COLUMN dupin INT NOT NULL DEFAULT 15;",
"UPDATE record SET dupmethod = 1 WHERE recorddups = 2;",
"UPDATE record SET dupin = 2 WHERE recorddups = 1;",
"ALTER TABLE record DROP COLUMN recorddups;",
"ALTER TABLE recorded ADD COLUMN recgroup VARCHAR(32) default 'Default';",
"ALTER TABLE recorded ADD COLUMN recordid INT DEFAULT NULL;",
"CREATE TABLE recgrouppassword ("
"  recgroup VARCHAR(32) NOT NULL PRIMARY KEY, "
"  password VARCHAR(10) NOT NULL, "
"  UNIQUE(recgroup)"
");",
""
};
        performActualUpdate(updates, "1029", dbver);
    }

    if (dbver == "1029")
    {
        const QString updates[] = {
"ALTER TABLE record CHANGE preroll startoffset INT DEFAULT 0 NOT NULL;",
"ALTER TABLE record CHANGE postroll endoffset INT DEFAULT 0 NOT NULL;",
""
};
        performActualUpdate(updates, "1030", dbver);
    }

    if (dbver == "1030") 
    {
        const QString updates[] = {
"ALTER TABLE channel ADD COLUMN visible TINYINT(1) NOT NULL default '1';",
"UPDATE channel SET visible = 1;",
""
};
        performActualUpdate(updates, "1031", dbver);
    }

    if (dbver == "1031") {
        const QString updates[] = {
"ALTER TABLE capturecard ADD dvb_on_demand TINYINT NOT NULL DEFAULT 0;",
""
};
        performActualUpdate(updates, "1032", dbver);
    }

    if (dbver == "1032")
    {
        const QString updates[] = {
"UPDATE record SET dupmethod = 6 WHERE dupmethod = 22;",
""
};
        performActualUpdate(updates, "1033", dbver);
    }

    if (dbver == "1033")
    {
        const QString updates[] = {
"ALTER TABLE program ADD title_pronounce VARCHAR(128) NULL;",
"ALTER TABLE program ADD INDEX (title_pronounce);",
""
};
        performActualUpdate(updates, "1034", dbver);
    }

    if (dbver == "1034")
    {
        const QString updates[] = {
"CREATE TABLE mythlog ("
"  logid int(10) unsigned PRIMARY KEY NOT NULL auto_increment,"
"  module char(32) NOT NULL,"
"  priority int(11) NOT NULL,"
"  acknowledged bool default 0,"
"  logdate datetime,"
"  host varchar(128),"
"  message varchar(255) NOT NULL,"
"  details text"
");",
"CREATE TABLE housekeeping ("
"  tag varchar(64) PRIMARY KEY NOT NULL,"
"  lastrun datetime"
");",
""
};
        performActualUpdate(updates, "1035", dbver);
    }
    
    if (dbver == "1035")
    {
        const QString updates[] = {
"ALTER TABLE dvb_sat CHANGE pos pos FLOAT;",
"ALTER TABLE dvb_sat ADD diseqc_pos SMALLINT DEFAULT 0 AFTER diseqc_port;",
""
};
        performActualUpdate(updates,"1036", dbver);
    }

    if (dbver == "1036")
    {
        const QString updates[] = {
"UPDATE channel SET callsign=chanid WHERE callsign IS NULL OR callsign='';",
"ALTER TABLE record ADD COLUMN station VARCHAR(20) NOT NULL DEFAULT '';",
"ALTER TABLE recordoverride ADD COLUMN station VARCHAR(20) NOT NULL DEFAULT '';",
""
};
        performActualUpdate(updates, "1037", dbver);
    }

    if (dbver == "1037")
    {
        const QString updates[] = {
"ALTER TABLE videosource ADD lineupid VARCHAR(64) NULL;",
"ALTER TABLE videosource ADD password VARCHAR(64) NULL;",
"ALTER TABLE program ADD ( "
"    stereo bool, "
"    subtitled bool, "
"    hdtv bool, "
"    closecaptioned bool, "
"    partnumber int, "
"    parttotal int, "
"    seriesid char(12), "
"    originalairdate date, "
"    showtype varchar(30), "
"    colorcode varchar(20), "
"    syndicatedepisodenumber varchar(20), "
"    programid char(12) "
");",
"DELETE FROM credits;",
"ALTER TABLE credits DROP INDEX chanid;",
"ALTER TABLE credits ADD UNIQUE chanid (chanid, starttime, person, role);",
"DELETE FROM people;",
"ALTER TABLE people DROP INDEX name;",
"ALTER TABLE people ADD UNIQUE name (name(41));",
"CREATE TABLE programgenres ( "
"    chanid int unsigned NOT NULL, "
"    starttime timestamp NOT NULL, "
"    relevance char(1) NOT NULL, "
"    genre char(30), "
"    PRIMARY KEY (chanid, starttime, relevance) "
");",
""
};
        performActualUpdate(updates, "1038", dbver);
    }

    if (dbver == "1038")
    {
        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS programgenres ( "
"    chanid int unsigned NOT NULL, "
"    starttime timestamp NOT NULL, "
"    relevance char(1) NOT NULL, "
"    genre char(30), "
"    PRIMARY KEY (chanid, starttime, relevance) "
");",
""
};
        performActualUpdate(updates, "1039", dbver);
    }

    if (dbver == "1039")
    {
        const QString updates[] = {
"ALTER TABLE channel CHANGE name name VARCHAR(64);",
""
};
        performActualUpdate(updates, "1040", dbver);
    }

    if (dbver == "1040")
    {
        const QString updates[] = {
"ALTER TABLE channel ADD outputfilters VARCHAR(255) NULL",
""
};
        performActualUpdate(updates, "1041", dbver);
    }

    if (dbver == "1041")
    {
        const QString updates[] = {
"ALTER TABLE record ADD seriesid varchar(12) NULL;",
"ALTER TABLE record ADD programid varchar(12) NULL;",
"ALTER TABLE recorded ADD seriesid varchar(12) NULL;",
"ALTER TABLE recorded ADD programid varchar(12) NULL;",
"ALTER TABLE oldrecorded ADD seriesid varchar(12) NULL;",
"ALTER TABLE oldrecorded ADD programid varchar(12) NULL;",
"ALTER TABLE program ADD INDEX (seriesid);",
"ALTER TABLE program ADD INDEX (programid);",
"ALTER TABLE record ADD INDEX (seriesid);",
"ALTER TABLE record ADD INDEX (programid);",
"ALTER TABLE recorded ADD INDEX (seriesid);",
"ALTER TABLE recorded ADD INDEX (programid);",
"ALTER TABLE oldrecorded ADD INDEX (seriesid);",
"ALTER TABLE oldrecorded ADD INDEX (programid);",
""
};
        performActualUpdate(updates, "1042", dbver);
    }

    if (dbver == "1042")
    {
        const QString updates[] = {
"INSERT INTO settings SET value=\"DataDirectMessage\";",
""
};
        performActualUpdate(updates, "1043", dbver);
    }

    if (dbver == "1043")
    {
        const QString updates[] = {
"ALTER TABLE program CHANGE title title VARCHAR(128) NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE subtitle subtitle VARCHAR(128) NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE description description TEXT NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE category category VARCHAR(64) NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE category_type category_type VARCHAR(64) NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE title_pronounce title_pronounce VARCHAR(128) NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE seriesid seriesid VARCHAR(12) NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE showtype showtype VARCHAR(30) NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE colorcode colorcode VARCHAR(20) NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE syndicatedepisodenumber syndicatedepisodenumber VARCHAR(20) NOT NULL DEFAULT '';",
"ALTER TABLE program CHANGE programid programid VARCHAR(12) NOT NULL DEFAULT '';",

"ALTER TABLE channel CHANGE channum channum VARCHAR(5) NOT NULL DEFAULT '';",
"ALTER TABLE channel CHANGE callsign callsign VARCHAR(20) NOT NULL DEFAULT '';",
"ALTER TABLE channel CHANGE name name VARCHAR(64) NOT NULL DEFAULT '';",
"ALTER TABLE channel CHANGE icon icon VARCHAR(255) NOT NULL DEFAULT 'none';",
"ALTER TABLE channel CHANGE videofilters videofilters VARCHAR(255) NOT NULL DEFAULT '';",
"ALTER TABLE channel CHANGE xmltvid xmltvid VARCHAR(64) NOT NULL DEFAULT '';",
"ALTER TABLE channel CHANGE outputfilters outputfilters VARCHAR(255) NOT NULL DEFAULT '';",

"ALTER TABLE record CHANGE title title VARCHAR(128) NOT NULL DEFAULT '';",
"ALTER TABLE record CHANGE subtitle subtitle VARCHAR(128) NOT NULL DEFAULT '';",
"ALTER TABLE record CHANGE description description TEXT NOT NULL DEFAULT '';",
"ALTER TABLE record CHANGE profile profile VARCHAR(128) NOT NULL DEFAULT 'Default';",
"ALTER TABLE record CHANGE category category VARCHAR(64) NOT NULL DEFAULT '';",
"ALTER TABLE record CHANGE recgroup recgroup VARCHAR(32) NOT NULL DEFAULT 'Default';",
"ALTER TABLE record CHANGE station station VARCHAR(20) NOT NULL DEFAULT '';",
"ALTER TABLE record CHANGE seriesid seriesid VARCHAR(12) NOT NULL DEFAULT '';",
"ALTER TABLE record CHANGE programid programid VARCHAR(12) NOT NULL DEFAULT '';",

"ALTER TABLE recorded CHANGE title title VARCHAR(128) NOT NULL DEFAULT '';",
"ALTER TABLE recorded CHANGE subtitle subtitle VARCHAR(128) NOT NULL DEFAULT '';",
"ALTER TABLE recorded CHANGE description description TEXT NOT NULL DEFAULT '';",
"ALTER TABLE recorded CHANGE hostname hostname VARCHAR(255) NOT NULL DEFAULT '';",
"ALTER TABLE recorded CHANGE category category VARCHAR(64) NOT NULL DEFAULT '';",
"ALTER TABLE recorded CHANGE recgroup recgroup VARCHAR(32) NOT NULL DEFAULT 'Default';",
"ALTER TABLE recorded CHANGE seriesid seriesid VARCHAR(12) NOT NULL DEFAULT '';",
"ALTER TABLE recorded CHANGE programid programid VARCHAR(12) NOT NULL DEFAULT '';",

"ALTER TABLE oldrecorded CHANGE title title VARCHAR(128) NOT NULL DEFAULT '';",
"ALTER TABLE oldrecorded CHANGE subtitle subtitle VARCHAR(128) NOT NULL DEFAULT '';",
"ALTER TABLE oldrecorded CHANGE description description TEXT NOT NULL DEFAULT '';",
"ALTER TABLE oldrecorded CHANGE category category VARCHAR(64) NOT NULL DEFAULT '';",
"ALTER TABLE oldrecorded CHANGE seriesid seriesid VARCHAR(12) NOT NULL DEFAULT '';",
"ALTER TABLE oldrecorded CHANGE programid programid VARCHAR(12) NOT NULL DEFAULT '';",
""
};
        performActualUpdate(updates, "1044", dbver);
    }

    if (dbver == "1044")
    {
        const QString updates[] = {
"UPDATE channel SET icon = 'none' WHERE icon = '';",
"UPDATE record SET profile = 'Default' WHERE profile = '';",
"UPDATE record SET recgroup = 'Default' WHERE recgroup = '';",
"UPDATE recorded SET recgroup = 'Default', starttime = starttime WHERE recgroup = '';",
""
};
        performActualUpdate(updates, "1045", dbver);
    }

    if (dbver == "1045")
    {
        const QString updates[] = {
"UPDATE recorded SET recgroup = 'Default', starttime = starttime WHERE recgroup = '';",
""
};
        performActualUpdate(updates, "1046", dbver);
    }

    if (dbver == "1046")
    {
        const QString updates[] = {
"ALTER TABLE record ADD COLUMN search INT UNSIGNED NOT NULL DEFAULT 0;",
"UPDATE record SET search = 0 WHERE search IS NULL;",
""
};
        performActualUpdate(updates, "1047", dbver);
    }

    if (dbver == "1047")
    {
        const QString updates[] = {
"CREATE TABLE networkiconmap ("
"    id INTEGER NOT NULL AUTO_INCREMENT,"
"    network VARCHAR(20) NOT NULL UNIQUE,"
"    url VARCHAR(255) NOT NULL,"
"    PRIMARY KEY(id)"
");",
"CREATE TABLE callsignnetworkmap ("
"    id INTEGER NOT NULL AUTO_INCREMENT,"
"    callsign VARCHAR(20) NOT NULL UNIQUE,"
"    network VARCHAR(20) NOT NULL,"
"    PRIMARY KEY(id)"
");",
""
};
        performActualUpdate(updates, "1048", dbver);
    }

    if (dbver == "1048")
    {
        const QString updates[] = {
"ALTER TABLE cardinput CHANGE preference preference INT NOT NULL DEFAULT 0;",
"UPDATE cardinput SET preference = 0 WHERE preference IS NULL;",
""
};
        performActualUpdate(updates, "1049", dbver);
    }

    if (dbver == "1049")
    {
        const QString updates[] = {
"ALTER TABLE keyword ADD COLUMN searchtype INT UNSIGNED NOT NULL DEFAULT 3;",
"ALTER TABLE keyword DROP INDEX phrase;",
"ALTER TABLE keyword DROP PRIMARY KEY;",
"ALTER TABLE keyword ADD UNIQUE(phrase,searchtype);",
""
};
        performActualUpdate(updates, "1050", dbver);
    }
    

    if (dbver == "1050")
    {
        const QString updates[] = {
"ALTER TABLE recorded CHANGE starttime starttime DATETIME NOT NULL;",
"ALTER TABLE recorded CHANGE endtime endtime DATETIME NOT NULL;",
"ALTER TABLE recorded ADD COLUMN lastmodified TIMESTAMP NOT NULL;",
"ALTER TABLE recorded ADD COLUMN filesize BIGINT(20) DEFAULT 0 NOT NULL;",
"ALTER TABLE credits CHANGE starttime starttime DATETIME NOT NULL;",
"ALTER TABLE oldprogram CHANGE airdate airdate DATETIME NOT NULL;",
"ALTER TABLE oldrecorded CHANGE starttime starttime DATETIME NOT NULL;",
"ALTER TABLE oldrecorded CHANGE endtime endtime DATETIME NOT NULL;",
"ALTER TABLE program CHANGE starttime starttime DATETIME NOT NULL;",
"ALTER TABLE program CHANGE endtime endtime DATETIME NOT NULL;",
"ALTER TABLE programgenres CHANGE starttime starttime DATETIME NOT NULL;",
"ALTER TABLE programrating CHANGE starttime starttime DATETIME NOT NULL;",
"ALTER TABLE recordedmarkup CHANGE starttime starttime DATETIME NOT NULL;",
"ALTER TABLE recordoverride CHANGE starttime starttime DATETIME NOT NULL;",
"ALTER TABLE recordoverride CHANGE endtime endtime DATETIME NOT NULL;",
"ALTER TABLE transcoding CHANGE starttime starttime DATETIME NOT NULL;",
""
};
        performActualUpdate(updates, "1051", dbver);
    }

    if (dbver == "1051")
    {
        const QString updates[] = {
"update record set dupmethod = 6 where dupmethod = 0;",
"update record set dupin = 15 where dupin = 0;",
""
};
        performActualUpdate(updates, "1052", dbver);
    }

    if (dbver == "1052")
    {
        const QString updates[] = {
"ALTER TABLE recorded ADD COLUMN stars FLOAT UNSIGNED NOT NULL DEFAULT 0;",
"ALTER TABLE recorded ADD COLUMN previouslyshown TINYINT(1) DEFAULT 0;",
"ALTER TABLE recorded ADD COLUMN originalairdate DATE;",
"INSERT INTO settings VALUES ('HaveRepeats', '0', NULL);",
""
};
        performActualUpdate(updates, "1053", dbver);
    }

    if (dbver == "1053")
    {
        const QString updates[] = {
"ALTER TABLE channel CHANGE freqid freqid VARCHAR(10);",
""
};
        performActualUpdate(updates, "1054", dbver);
    }

    if (dbver == "1054")
    {
        const QString updates[] = {
"ALTER TABLE program ADD INDEX id_start_end (chanid,starttime,endtime);",
"ALTER TABLE channel ADD INDEX channel_src (channum,sourceid);",
""
};
        performActualUpdate(updates, "1055", dbver);
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
        exit(-14);
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

