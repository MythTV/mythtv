#include <qsqldatabase.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythcontext.h"
#include "mythdbcon.h"

const QString currentDatabaseVersion = "1080";

static bool UpdateDBVersionNumber(const QString &newnumber);
static bool performActualUpdate(const QString updates[], QString version,
                                QString &dbver);
static bool InitializeDatabase(void);
bool doUpgradeTVDatabaseSchema(void);

static bool UpdateDBVersionNumber(const QString &newnumber)
{
    // delete old schema version
    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery = "DELETE FROM settings WHERE value='DBSchemaVer';";
    query.prepare(thequery);
    query.exec();

    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (Deleting old DB version number): \n"
                    "Query was: %1 \nError was: %2 \nnew version: %3")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()))
            .arg(newnumber);
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }

    // set new schema version
    thequery = QString("INSERT INTO settings (value, data, hostname) "
                       "VALUES ('DBSchemaVer', %1, NULL);").arg(newnumber);
    query.prepare(thequery);
    query.exec();

    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (Setting new DB version number): \n"
                    "Query was: %1 \nError was: %2 \nnew version: %3")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()))
            .arg(newnumber);
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }

    return true;
}

static bool performActualUpdate(const QString updates[], QString version,
                         QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_ALL, QString("Upgrading to schema version ") + version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        query.prepare(thequery);
        query.exec();

        if (query.lastError().type() != QSqlError::None)
        {
            QString msg =
                QString("DB Error (Performing database upgrade): \n"
                        "Query was: %1 \nError was: %2 \nnew version: %3")
                .arg(thequery)
                .arg(MythContext::DBErrorMessage(query.lastError()))
                .arg(version);
            VERBOSE(VB_IMPORTANT, msg);
            return false;
        }

        counter++;
        thequery = updates[counter];
    }

    if (!UpdateDBVersionNumber(version))
        return false;

    dbver = version;
    return true;
}

bool UpgradeTVDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("DBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    MSqlQuery lockquery(MSqlQuery::InitCon());

    lockquery.prepare("CREATE TABLE IF NOT EXISTS "
                      "schemalock ( schemalock int(1));");
    if (!lockquery.exec())
    {
        VERBOSE(VB_IMPORTANT, QString("ERROR: Unable to create database "
                                      "upgrade lock table: %1")
                                      .arg(MythContext::DBErrorMessage(
                                           lockquery.lastError())));
        return false;
    }

    VERBOSE(VB_IMPORTANT, "Setting Lock for Database Schema upgrade. If you "
                          "see a long pause here it means the Schema is "
                          "already locked and is being upgraded by another "
                          "Myth process.");

    lockquery.prepare("LOCK TABLE schemalock WRITE;");
    if (!lockquery.exec())
    {
        VERBOSE(VB_IMPORTANT, QString("ERROR: Unable to acquire database "
                                      "upgrade lock")
                                      .arg(MythContext::DBErrorMessage(
                                           lockquery.lastError())));
        return false;
    }

    bool ret = doUpgradeTVDatabaseSchema();

    if (ret)
        VERBOSE(VB_IMPORTANT, "Database Schema upgrade complete, unlocking.");
    else
        VERBOSE(VB_IMPORTANT, "Database Schema upgrade FAILED, unlocking.");

    lockquery.prepare("UNLOCK TABLES;");
    lockquery.exec();

    return ret;
}

bool doUpgradeTVDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("DBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;
    
    if (dbver == "")
    {
        if (!InitializeDatabase())
            return false;
        dbver = "1060";
    }

    if (dbver == "900")
    {
        const QString updates[] = {
"ALTER TABLE program ADD COLUMN category_type VARCHAR(64) NULL;",
"DROP TABLE IF EXISTS transcoding;",
"CREATE TABLE transcoding (chanid INT UNSIGNED, starttime TIMESTAMP, status INT, hostname VARCHAR(255));",
""
};
        if (!performActualUpdate(updates, "901", dbver))
            return false;
    }

    if (dbver == "901")
    {
        const QString updates[] = {
"ALTER TABLE record ADD COLUMN category VARCHAR(64) NULL;",
"ALTER TABLE recorded ADD COLUMN category VARCHAR(64) NULL;",
"ALTER TABLE oldrecorded ADD COLUMN category VARCHAR(64) NULL;",
""
};
        if (!performActualUpdate(updates, "902", dbver))
            return false;
    }

    if (dbver == "902")
    {
        const QString updates[] = {
"ALTER TABLE record ADD rank INT(10) DEFAULT '0' NOT NULL;",
"ALTER TABLE channel ADD rank INT(10) DEFAULT '0' NOT NULL;",
""
};
        if (!performActualUpdate(updates, "903", dbver))
            return false;
    }

    if (dbver == "903")
    {
        const QString updates[] = {
"ALTER TABLE channel ADD COLUMN freqid VARCHAR(5) NOT NULL;",
"UPDATE channel set freqid=channum;",
"ALTER TABLE channel ADD hue INT DEFAULT '32768';",
""
};
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        const QString updates[] = {
"ALTER TABLE record ADD autoexpire INT DEFAULT 0 NOT NULL;",
"ALTER TABLE recorded ADD autoexpire INT DEFAULT 0 NOT NULL;",
""
};
        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }

    if (dbver == "1001")
    {
        const QString updates[] = {
"ALTER TABLE record ADD maxepisodes INT DEFAULT 0 NOT NULL;",
""
};
        if (!performActualUpdate(updates, "1002", dbver))
            return false;
    }

    if (dbver == "1002")
    {
        const QString updates[] = {
"ALTER TABLE record ADD recorddups INT DEFAULT 0 NOT NULL;",
"ALTER TABLE record ADD maxnewest INT DEFAULT 0 NOT NULL;",
""
};
        if (!performActualUpdate(updates, "1003", dbver))
            return false;
    }

    if (dbver == "1003")
    {
        const QString updates[] = {
""
};
        if (!performActualUpdate(updates, "1004", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1005", dbver))
            return false;
    }

    if (dbver == "1005")
    {
        const QString updates[] = {
"DELETE FROM settings where value = 'TranscoderAutoRun';",
"ALTER TABLE record CHANGE profile profile VARCHAR(128);",
""
};
        if (!performActualUpdate(updates, "1006", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1007", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1008", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1009", dbver))
            return false;
    }

    if (dbver == "1009")
    {
        const QString updates[] = {
"ALTER TABLE record ADD COLUMN preroll INT DEFAULT 0 NOT NULL;",
"ALTER TABLE record ADD COLUMN postroll INT DEFAULT 0 NOT NULL;",
""
};
        if (!performActualUpdate(updates, "1010", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1011", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1012", dbver))
            return false;
    }

    if (dbver == "1012")
    {
        const QString updates[] = {
"INSERT INTO settings SET value=\"mythfilldatabaseLastRunStart\";",
"INSERT INTO settings SET value=\"mythfilldatabaseLastRunEnd\";",
"INSERT INTO settings SET value=\"mythfilldatabaseLastRunStatus\";",
""
};
        if (!performActualUpdate(updates, "1013", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1014", dbver))
            return false;
    }

    if (dbver == "1014")
    {
        const QString updates[] = {
"ALTER TABLE dvb_sat CHANGE lnb_lof_switch lnb_lof_switch INTEGER DEFAULT 11700000;",
"UPDATE settings SET value='RecPriorityOrder' WHERE value='RecPriorityingOrder';",
""
};

        if (!performActualUpdate(updates, "1015", dbver))
            return false;
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

        if (!performActualUpdate(updates, "1016", dbver))
            return false;
    }

    if (dbver == "1016")
    {
        const QString updates[] = {
"ALTER TABLE jumppoints ADD PRIMARY KEY (destination, hostname);",
"ALTER TABLE keybindings ADD PRIMARY KEY (context, action, hostname);",
""
};

        if (!performActualUpdate(updates, "1017", dbver))
            return false;
    }

    if (dbver == "1017")
    {
        const QString updates[] = {
"UPDATE settings SET value='RecPriorityActive' WHERE value='RecPriorityingActive';",
""
};

        if (!performActualUpdate(updates, "1018", dbver))
            return false;
    }

    if (dbver == "1018")
    {
        const QString updates[] = {
"ALTER TABLE channel ADD COLUMN tvformat VARCHAR(10) NOT NULL default 'Default';",
""
};

        if (!performActualUpdate(updates, "1019", dbver))
            return false;
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

        if (!performActualUpdate(updates, "1020", dbver))
            return false;
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

        if (!performActualUpdate(updates, "1021", dbver))
            return false;
    }

    if (dbver == "1021")
    {
        const QString updates[] = {
"ALTER TABLE recorded ADD COLUMN commflagged int(10) unsigned NOT NULL default '0';",
""
};

        if (!performActualUpdate(updates, "1022", dbver))
            return false;

        VERBOSE(VB_ALL, QString("This could take a few minutes."));


        QString thequery, chanid, startts;
        QDateTime recstartts;

        MSqlQuery query(MSqlQuery::InitCon());

        thequery = QString("SELECT distinct recorded.chanid, "
                           "recorded.starttime "
                           "FROM recorded, recordedmarkup "
                           "WHERE recorded.chanid = recordedmarkup.chanid AND "
                           "recorded.starttime = recordedmarkup.starttime AND "
                           "(type = 4 OR type = 5) ");
        query.prepare(thequery);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            MSqlQuery query2(MSqlQuery::InitCon());
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
                query2.prepare(thequery);
                query2.exec();
            }  
        }
    }

    if (dbver == "1022")
    {
        const QString updates[] = {
"ALTER TABLE dvb_sat CHANGE lnb_lof_switch lnb_lof_switch INTEGER DEFAULT 11700000;",
""
};
        if (!performActualUpdate(updates, "1023", dbver))
            return false;
    }

    if (dbver == "1023")
    {
        const QString updates[] = {
QString("ALTER TABLE videosource ADD COLUMN freqtable VARCHAR(16) NOT NULL DEFAULT 'default';"),
""
};
        if (!performActualUpdate(updates, "1024", dbver))
            return false;
    }

    if (dbver == "1024")
    {
        const QString updates[] = {
"ALTER TABLE dvb_sat CHANGE sourceid cardid INT;",
"ALTER TABLE capturecard ADD COLUMN dvb_sat_type INT NOT NULL DEFAULT 0;",
"ALTER TABLE dvb_channel ADD COLUMN pmtcache BLOB;",
""
};
        if (!performActualUpdate(updates, "1025", dbver))
            return false;
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

        if (!performActualUpdate(updates, "1026", dbver))
            return false;
    }

    if (dbver == "1026")
    {
        const QString updates[] = {
"ALTER TABLE capturecard ADD COLUMN dvb_wait_for_seqstart INT NOT NULL DEFAULT 1;",
"ALTER TABLE capturecard ADD COLUMN dvb_dmx_buf_size INT NOT NULL DEFAULT 8192;",
"ALTER TABLE capturecard ADD COLUMN dvb_pkt_buf_size INT NOT NULL DEFAULT 8192;",
""
};
        if (!performActualUpdate(updates, "1027", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1028", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1029", dbver))
            return false;
    }

    if (dbver == "1029")
    {
        const QString updates[] = {
"ALTER TABLE record CHANGE preroll startoffset INT DEFAULT 0 NOT NULL;",
"ALTER TABLE record CHANGE postroll endoffset INT DEFAULT 0 NOT NULL;",
""
};
        if (!performActualUpdate(updates, "1030", dbver))
            return false;
    }

    if (dbver == "1030") 
    {
        const QString updates[] = {
"ALTER TABLE channel ADD COLUMN visible TINYINT(1) NOT NULL default '1';",
"UPDATE channel SET visible = 1;",
""
};
        if (!performActualUpdate(updates, "1031", dbver))
            return false;
    }

    if (dbver == "1031") {
        const QString updates[] = {
"ALTER TABLE capturecard ADD dvb_on_demand TINYINT NOT NULL DEFAULT 0;",
""
};
        if (!performActualUpdate(updates, "1032", dbver))
            return false;
    }

    if (dbver == "1032")
    {
        const QString updates[] = {
"UPDATE record SET dupmethod = 6 WHERE dupmethod = 22;",
""
};
        if (!performActualUpdate(updates, "1033", dbver))
            return false;
    }

    if (dbver == "1033")
    {
        const QString updates[] = {
"ALTER TABLE program ADD title_pronounce VARCHAR(128) NULL;",
"ALTER TABLE program ADD INDEX (title_pronounce);",
""
};
        if (!performActualUpdate(updates, "1034", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1035", dbver))
            return false;
    }
    
    if (dbver == "1035")
    {
        const QString updates[] = {
"ALTER TABLE dvb_sat CHANGE pos pos FLOAT;",
"ALTER TABLE dvb_sat ADD diseqc_pos SMALLINT DEFAULT 0 AFTER diseqc_port;",
""
};
        if (!performActualUpdate(updates,"1036", dbver))
            return false;
    }

    if (dbver == "1036")
    {
        const QString updates[] = {
"UPDATE channel SET callsign=chanid WHERE callsign IS NULL OR callsign='';",
"ALTER TABLE record ADD COLUMN station VARCHAR(20) NOT NULL DEFAULT '';",
"ALTER TABLE recordoverride ADD COLUMN station VARCHAR(20) NOT NULL DEFAULT '';",
""
};
        if (!performActualUpdate(updates, "1037", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1038", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1039", dbver))
            return false;
    }

    if (dbver == "1039")
    {
        const QString updates[] = {
"ALTER TABLE channel CHANGE name name VARCHAR(64);",
""
};
        if (!performActualUpdate(updates, "1040", dbver))
            return false;
    }

    if (dbver == "1040")
    {
        const QString updates[] = {
"ALTER TABLE channel ADD outputfilters VARCHAR(255) NULL",
""
};
        if (!performActualUpdate(updates, "1041", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1042", dbver))
            return false;
    }

    if (dbver == "1042")
    {
        const QString updates[] = {
"INSERT INTO settings SET value=\"DataDirectMessage\";",
""
};
        if (!performActualUpdate(updates, "1043", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1044", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1045", dbver))
            return false;
    }

    if (dbver == "1045")
    {
        const QString updates[] = {
"UPDATE recorded SET recgroup = 'Default', starttime = starttime WHERE recgroup = '';",
""
};
        if (!performActualUpdate(updates, "1046", dbver))
            return false;
    }

    if (dbver == "1046")
    {
        const QString updates[] = {
"ALTER TABLE record ADD COLUMN search INT UNSIGNED NOT NULL DEFAULT 0;",
"UPDATE record SET search = 0 WHERE search IS NULL;",
""
};
        if (!performActualUpdate(updates, "1047", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1048", dbver))
            return false;
    }

    if (dbver == "1048")
    {
        const QString updates[] = {
"ALTER TABLE cardinput CHANGE preference preference INT NOT NULL DEFAULT 0;",
"UPDATE cardinput SET preference = 0 WHERE preference IS NULL;",
""
};
        if (!performActualUpdate(updates, "1049", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1050", dbver))
            return false;
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
        if (!performActualUpdate(updates, "1051", dbver))
            return false;
    }

    if (dbver == "1051")
    {
        const QString updates[] = {
"update record set dupmethod = 6 where dupmethod = 0;",
"update record set dupin = 15 where dupin = 0;",
""
};
        if (!performActualUpdate(updates, "1052", dbver))
            return false;
    }

    if (dbver == "1052")
    {
        const QString updates[] = {
"ALTER TABLE recorded ADD COLUMN stars FLOAT NOT NULL DEFAULT 0;",
"ALTER TABLE recorded ADD COLUMN previouslyshown TINYINT(1) DEFAULT 0;",
"ALTER TABLE recorded ADD COLUMN originalairdate DATE;",
"INSERT INTO settings VALUES ('HaveRepeats', '0', NULL);",
""
};
        if (!performActualUpdate(updates, "1053", dbver))
            return false;
    }

    if (dbver == "1053")
    {
        const QString updates[] = {
"ALTER TABLE channel CHANGE freqid freqid VARCHAR(10);",
""
};
        if (!performActualUpdate(updates, "1054", dbver))
            return false;
    }

    if (dbver == "1054")
    {
        const QString updates[] = {
"ALTER TABLE program ADD INDEX id_start_end (chanid,starttime,endtime);",
"ALTER TABLE channel ADD INDEX channel_src (channum,sourceid);",
""
};
        if (!performActualUpdate(updates, "1055", dbver))
            return false;
    }

    if (dbver == "1055")
    {
        const QString updates[] = {
"UPDATE record SET dupmethod=6, dupin=4 WHERE dupmethod=8;",
""
};
        if (!performActualUpdate(updates, "1056", dbver))
            return false;
    }

    if (dbver == "1056")
    {
        const QString updates[] = {
"CREATE TABLE jobqueue ("
"    id INTEGER NOT NULL AUTO_INCREMENT,"
"    chanid INTEGER(10) NOT NULL,"
"    starttime DATETIME NOT NULL,"
"    inserttime DATETIME NOT NULL,"
"    type INTEGER NOT NULL,"
"    cmds INTEGER NOT NULL DEFAULT 0,"
"    flags INTEGER NOT NULL DEFAULT 0,"
"    status INTEGER NOT NULL DEFAULT 0,"
"    statustime TIMESTAMP NOT NULL,"
"    hostname VARCHAR(255) NOT NULL DEFAULT '',"
"    args BLOB NOT NULL DEFAULT '',"
"    comment VARCHAR(128) NOT NULL DEFAULT '',"
"    PRIMARY KEY(id),"
"    UNIQUE(chanid, starttime, type, inserttime)"
");",
"ALTER TABLE record ADD COLUMN autotranscode TINYINT(1) NOT NULL DEFAULT 0;",
"ALTER TABLE record ADD COLUMN autocommflag TINYINT(1) NOT NULL DEFAULT 0;",
"ALTER TABLE record ADD COLUMN autouserjob1 TINYINT(1) NOT NULL DEFAULT 0;",
"ALTER TABLE record ADD COLUMN autouserjob2 TINYINT(1) NOT NULL DEFAULT 0;",
"ALTER TABLE record ADD COLUMN autouserjob3 TINYINT(1) NOT NULL DEFAULT 0;",
"ALTER TABLE record ADD COLUMN autouserjob4 TINYINT(1) NOT NULL DEFAULT 0;",
""
};
        if (!performActualUpdate(updates, "1057", dbver))
            return false;

        if (gContext->GetNumSetting("AutoCommercialFlag", 1))
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("UPDATE record SET autocommflag = 1;");
            query.exec();
        }
    }
    if (dbver == "1057")
    {
        const QString updates[] = {
"DROP TABLE IF EXISTS transcoding;",
""
};
        if (!performActualUpdate(updates, "1058", dbver))
            return false;
    }

    if (dbver == "1058")
    {
        const QString updates[] = {
"UPDATE program SET category_type='series' WHERE showtype LIKE '%series%';",
""
};
        if (!performActualUpdate(updates, "1059", dbver))
            return false;
    }

    if (dbver == "1059")
    {
        const QString updates[] = {
"ALTER TABLE recorded ADD COLUMN preserve TINYINT(1) NOT NULL DEFAULT 0;",
""
};
        if (!performActualUpdate(updates, "1060", dbver))
            return false;
    }

    if (dbver == "1060")
    {
        const QString updates[] = {

"ALTER TABLE record ADD COLUMN record.findday TINYINT NOT NULL DEFAULT 0;",
"ALTER TABLE record ADD COLUMN record.findtime TIME NOT NULL DEFAULT '00:00:00';",
"ALTER TABLE record ADD COLUMN record.findid INT NOT NULL DEFAULT 0;",
"ALTER TABLE recorded ADD COLUMN recorded.findid INT NOT NULL DEFAULT 0;",
"ALTER TABLE oldrecorded ADD COLUMN oldrecorded.findid INT NOT NULL DEFAULT 0;",
""
};
        if (!performActualUpdate(updates, "1061", dbver))
            return false;
    }

    if (dbver == "1061")
    {
        const QString updates[] = {
"ALTER TABLE record ADD COLUMN inactive TINYINT(1) NOT NULL DEFAULT 0;",
""
};
        if (!performActualUpdate(updates, "1062", dbver))
            return false;
    }

    if (dbver == "1062")
    {
        const QString updates[] = {
"ALTER TABLE cardinput ADD COLUMN freetoaironly TINYINT(1) DEFAULT 1;",
"ALTER TABLE channel ADD COLUMN useonairguide TINYINT(1) DEFAULT 0;",
"ALTER TABLE capturecard ADD COLUMN dvb_diseqc_type SMALLINT(6);",
"ALTER TABLE cardinput ADD COLUMN diseqc_port SMALLINT(6);",
"ALTER TABLE cardinput ADD COLUMN diseqc_pos FLOAT;",
"ALTER TABLE cardinput ADD COLUMN lnb_lof_switch INT(11) DEFAULT 11700000;",
"ALTER TABLE cardinput ADD COLUMN lnb_lof_hi INT(11) DEFAULT 10600000;",
"ALTER TABLE cardinput ADD COLUMN lnb_lof_lo INT(11) DEFAULT 9750000;",
"ALTER TABLE channel ADD COLUMN mplexid SMALLINT(6);",
"ALTER TABLE channel ADD COLUMN serviceid SMALLINT(6);",
"ALTER TABLE channel ADD COLUMN atscsrcid INT(11) DEFAULT NULL;",
"CREATE TABLE dtv_multiplex ("
"  mplexid smallint(6) NOT NULL auto_increment, "
"  sourceid smallint(6) default NULL,"
"  transportid int(11) default NULL,"
"  networkid int(11) default NULL,"
"  frequency int(11) default NULL,"
"  inversion char(1) default 'a',"
"  symbolrate int(11) default NULL,"
"  fec varchar(10) default 'auto',"
"  polarity char(1) default NULL,"
"  modulation varchar(10) default 'auto',"
"  bandwidth char(1) default 'a',"
"  lp_code_rate varchar(10) default 'auto',"
"  transmission_mode char(1) default 'a',"
"  guard_interval varchar(10) default 'auto',"
"  visible smallint(1) NOT NULL default '0',"
"  constellation varchar(10) default 'auto',"
"  hierarchy varchar(10) default 'auto',"
"  hp_code_rate varchar(10) default 'auto',"
"  sistandard varchar(10) default 'dvb',"
"  serviceversion smallint(6) default 33,"
"  updatetimestamp timestamp(14) NOT NULL,"
"  PRIMARY KEY  (mplexid)"
") TYPE=MyISAM;",
// These should be included in an update after the 0.17 release.
// "DROP TABLE IF EXISTS dvb_channel;",
// "DROP TABLE IF EXISTS dvb_pids;",
// "DROP TABLE IF EXISTS dvb_sat;",
"CREATE TABLE dtv_privatetypes ("
"  sitype varchar(4) NOT NULL, "
"  networkid int(11) NOT NULL, "
"  private_type varchar(20) NOT NULL, "
"  private_value varchar(100) NOT NULL "
");",
//# UK DVB-T
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',9018,'channel_numbers','131');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',9018,'guide_fixup','2');",
//# Bell ExpressVu Canada
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',256,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',257,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',256,'tv_types','1,150,134,133');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',257,'tv_types','1,150,134,133');",

"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4100,'sdt_mapping','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4101,'sdt_mapping','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4102,'sdt_mapping','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4103,'sdt_mapping','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4104,'sdt_mapping','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4105,'sdt_mapping','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4106,'sdt_mapping','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4107,'sdt_mapping','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4097,'sdt_mapping','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4098,'sdt_mapping','1');",

"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4100,'tv_types','1,145,154');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4101,'tv_types','1,145,154');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4102,'tv_types','1,145,154');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4103,'tv_types','1,145,154');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4104,'tv_types','1,145,154');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4105,'tv_types','1,145,154');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4106,'tv_types','1,145,154');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4107,'tv_types','1,145,154');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4097,'tv_types','1,145,154');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4098,'tv_types','1,145,154');",

"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4100,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4101,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4102,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4103,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4104,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4105,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4106,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4107,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4097,'guide_fixup','1');",
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4098,'guide_fixup','1');",

//# NSAB / Sirius
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',94,'tv_types','1,128');",
//# WUNC Guide
"INSERT into dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('atsc',1793,'guide_fixup','3');",
""
};
        if (!performActualUpdate(updates, "1063", dbver))
            return false;
    }

    if (dbver == "1063")
    {
        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS recordmatch (recordid int unsigned, "
"chanid int unsigned, starttime datetime, INDEX (recordid));",
""
};
        if (!performActualUpdate(updates, "1064", dbver))
            return false;
    }

    if (dbver == "1064")
    {
        const QString updates[] = {
"ALTER TABLE `program` CHANGE `stereo` `stereo` TINYINT( 1 ) DEFAULT '0' NOT NULL;",
"ALTER TABLE `program` CHANGE `subtitled` `subtitled` TINYINT( 1 ) DEFAULT '0' NOT NULL;",
"ALTER TABLE `program` CHANGE `hdtv` `hdtv` TINYINT( 1 ) DEFAULT '0' NOT NULL;",
"ALTER TABLE `program` CHANGE `closecaptioned` `closecaptioned` TINYINT( 1 ) DEFAULT '0' NOT NULL;",
"ALTER TABLE `program` CHANGE `partnumber` `partnumber` INT( 11 ) DEFAULT '0' NOT NULL;",
"ALTER TABLE `program` CHANGE `parttotal` `parttotal` INT( 11 ) DEFAULT '0' NOT NULL;",
"ALTER TABLE `program` CHANGE `programid` `programid` VARCHAR( 20 ) NOT NULL;",
"ALTER TABLE `oldrecorded` CHANGE `programid` `programid` VARCHAR( 20 ) NOT NULL;",
"ALTER TABLE `recorded` CHANGE `programid` `programid` VARCHAR( 20 ) NOT NULL;",
"ALTER TABLE `record` CHANGE `programid` `programid` VARCHAR( 20 ) NOT NULL;",
""
};
        if (!performActualUpdate(updates, "1065", dbver))
            return false;
    }
  
    if (dbver == "1065") 
    {
        const QString updates[] = {
"INSERT INTO profilegroups SET name = 'FireWire Input', cardtype = 'FIREWIRE', is_default = 1;",
"ALTER TABLE capturecard ADD COLUMN firewire_port INT UNSIGNED NOT NULL DEFAULT 0;",
"ALTER TABLE capturecard ADD COLUMN firewire_node INT UNSIGNED NOT NULL DEFAULT 2;",
"ALTER TABLE capturecard ADD COLUMN firewire_speed INT UNSIGNED NOT NULL DEFAULT 0;",
"ALTER TABLE capturecard ADD COLUMN firewire_model varchar(32) NULL;",
""
};
        if (!performActualUpdate(updates, "1066", dbver))
            return false;
    } 
 
    if (dbver == "1066")
    {
        const QString updates[] = {
"INSERT INTO dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES('dvb', '40999', 'guide_fixup', '4');",
""
};
        if (!performActualUpdate(updates, "1067", dbver))
            return false;
    }

    if (dbver == "1067")
    {
        const QString updates[] = {
"INSERT INTO dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',70,'force_guide_present','yes');",
"INSERT INTO dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',70,'guide_ranges','80,80,96,96');",
"INSERT INTO dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4112,'channel_numbers','131');",
"INSERT INTO dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4115,'channel_numbers','131');",
"INSERT INTO dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',4116,'channel_numbers','131');",
"INSERT INTO dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',12802,'channel_numbers','131');",
"INSERT INTO dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',12803,'channel_numbers','131');",
"INSERT INTO dtv_privatetypes (sitype,networkid,private_type,private_value) VALUES ('dvb',12829,'channel_numbers','131');",
""
};
        if (!performActualUpdate(updates, "1068", dbver))
            return false;
    }

    if (dbver == "1068")
    {
        const QString updates[] = {
"ALTER TABLE recorded ADD COLUMN deletepending TINYINT(1) NOT NULL DEFAULT 0;",
""
};
        if (!performActualUpdate(updates, "1069", dbver))
            return false;
    }

    if (dbver == "1069")
    {
        const QString updates[] = {
"UPDATE jumppoints SET description = 'Weather forecasts' "
    "WHERE description = 'Weather forcasts';",
""
};
        if (!performActualUpdate(updates, "1070", dbver))
            return false;
    }

    if (dbver == "1070")
    {
        const QString updates[] = {
"UPDATE recorded SET bookmark = NULL WHERE bookmark = 'NULL';",
""
};
        if (!performActualUpdate(updates, "1071", dbver))
            return false;
    }

    if (dbver == "1071")
    {
        const QString updates[] = {
"ALTER TABLE program ADD COLUMN manualid INT UNSIGNED NOT NULL DEFAULT 0;",
"ALTER TABLE program DROP PRIMARY KEY;",
"ALTER TABLE program ADD PRIMARY KEY (chanid, starttime, manualid);",
"ALTER TABLE recordmatch ADD COLUMN manualid INT UNSIGNED;",
""
};
        if (!performActualUpdate(updates, "1072", dbver))
            return false;
    }

    if (dbver == "1072")
    {
        const QString updates[] = {
"ALTER TABLE recorded ADD INDEX (title);",
""
};
        if (!performActualUpdate(updates, "1073", dbver))
            return false;
    }

    if (dbver == "1073")
    {
        const QString updates[] = {
"ALTER TABLE capturecard ADD COLUMN firewire_connection INT UNSIGNED NOT NULL DEFAULT 0;",
""
};
        if (!performActualUpdate(updates, "1074", dbver))
            return false;
    }

    if (dbver == "1074")
    {
        const QString updates[] = {
"INSERT INTO profilegroups SET name = \"USB Mpeg-4 Encoder (Plextor ConvertX, etc)\", cardtype = 'GO7007', is_default = 1;",
"INSERT INTO recordingprofiles SET name = \"Default\", profilegroup = 8;",
"INSERT INTO recordingprofiles SET name = \"Live TV\", profilegroup = 8;",
"INSERT INTO recordingprofiles SET name = \"High Quality\", profilegroup = 8;",
"INSERT INTO recordingprofiles SET name = \"Low Quality\", profilegroup = 8;",
""
};
        if (!performActualUpdate(updates, "1075", dbver))
            return false;
    }

    if (dbver = "1075")
    {
        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS recordedprogram ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  endtime datetime NOT NULL default '0000-00-00 00:00:00',"
"  title varchar(128) NOT NULL default '',"
"  subtitle varchar(128) NOT NULL default '',"
"  description text NOT NULL,"
"  category varchar(64) NOT NULL default '',"
"  category_type varchar(64) NOT NULL default '',"
"  airdate year(4) NOT NULL default '0000',"
"  stars float unsigned NOT NULL default '0',"
"  previouslyshown tinyint(4) NOT NULL default '0',"
"  title_pronounce varchar(128) NOT NULL default '',"
"  stereo tinyint(1) NOT NULL default '0',"
"  subtitled tinyint(1) NOT NULL default '0',"
"  hdtv tinyint(1) NOT NULL default '0',"
"  closecaptioned tinyint(1) NOT NULL default '0',"
"  partnumber int(11) NOT NULL default '0',"
"  parttotal int(11) NOT NULL default '0',"
"  seriesid varchar(12) NOT NULL default '',"
"  originalairdate date default NULL,"
"  showtype varchar(30) NOT NULL default '',"
"  colorcode varchar(20) NOT NULL default '',"
"  syndicatedepisodenumber varchar(20) NOT NULL default '',"
"  programid varchar(20) NOT NULL default '',"
"  manualid int(10) unsigned NOT NULL default '0',"
"  PRIMARY KEY  (chanid,starttime,manualid),"
"  KEY endtime (endtime),"
"  KEY title (title),"
"  KEY title_pronounce (title_pronounce),"
"  KEY seriesid (seriesid),"
"  KEY programid (programid),"
"  KEY id_start_end (chanid,starttime,endtime)"
");",
"CREATE TABLE IF NOT EXISTS recordedcredits ("
"  person mediumint(8) unsigned NOT NULL default '0',"
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  role set('actor','director','producer','executive_producer','writer','guest_star','host','adapter','presenter','commentator','guest') NOT NULL default '',"
"  UNIQUE KEY chanid (chanid,starttime,person,role),"
"  KEY person (person,role)"
");",
"CREATE TABLE IF NOT EXISTS recordedrating ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  system char(8) NOT NULL default '',"
"  rating char(8) NOT NULL default '',"
"  UNIQUE KEY chanid (chanid,starttime,system,rating),"
"  KEY starttime (starttime,system)"
");",
""
       };
       
        if (!performActualUpdate(updates, "1076", dbver))
            return false;
    }

    if (dbver == "1076")
    {
        const QString updates[] = {
"ALTER TABLE channel MODIFY COLUMN serviceid mediumint unsigned;",
""
        }; 
        if (!performActualUpdate(updates, "1077", dbver))
            return false;
    }

    if (dbver == "1077")
    {
        const QString updates[] = {
"INSERT INTO `dtv_privatetypes` "
"(`sitype`,`networkid`,`private_type`,`private_value`) VALUES "
"('dvb',40999,'parse_subtitle_list',"
"'1070,1049,1041,1039,1038,1030,1016,1131,1068,1069');",
""
        }; 
        if (!performActualUpdate(updates, "1078", dbver))
            return false;
    }

    if (dbver == "1078")
    {
        const QString updates[] = {
"ALTER TABLE capturecard ADD COLUMN dvb_hw_decoder INT DEFAULT '0';",
""
};
        if (!performActualUpdate(updates, "1079", dbver))
            return false;
    }

    if (dbver == "1079")
    {
        const QString updates[] = {
"ALTER TABLE oldrecorded ADD COLUMN recordid INT NOT NULL DEFAULT 0;",
""
};
        if (!performActualUpdate(updates, "1080", dbver))
            return false;
    }

    return true;
}

bool InitializeDatabase(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SHOW TABLES;");

    // check for > 1 table here since the schemalock table should exist
    if (query.exec() && query.isActive() && query.size() > 1)
    {
        QString msg = QString(
            "Told to create a NEW database schema, but the database\n"
            "already has %1 tables.\n"
            "If you are sure this is a good mythtv database, verify\n"
            "that the settings table has the DBSchemaVer variable.\n")
            .arg(query.size() - 1);
        VERBOSE(VB_ALL, msg);
        return false;   
    }

    VERBOSE(VB_ALL, "Inserting MythTV initial database information.");

    const QString updates[] = {
"CREATE TABLE IF NOT EXISTS callsignnetworkmap ("
"  id int(11) NOT NULL auto_increment,"
"  callsign varchar(20) NOT NULL default '',"
"  network varchar(20) NOT NULL default '',"
"  PRIMARY KEY  (id),"
"  UNIQUE KEY callsign (callsign)"
");",
"CREATE TABLE IF NOT EXISTS capturecard ("
"  cardid int(10) unsigned NOT NULL auto_increment,"
"  videodevice varchar(128) default NULL,"
"  audiodevice varchar(128) default NULL,"
"  vbidevice varchar(128) default NULL,"
"  cardtype varchar(32) default 'V4L',"
"  defaultinput varchar(32) default 'Television',"
"  audioratelimit int(11) default NULL,"
"  hostname varchar(255) default NULL,"
"  dvb_swfilter int(11) default '0',"
"  dvb_recordts int(11) default '0',"
"  dvb_sat_type int(11) NOT NULL default '0',"
"  dvb_wait_for_seqstart int(11) NOT NULL default '1',"
"  dvb_dmx_buf_size int(11) NOT NULL default '8192',"
"  dvb_pkt_buf_size int(11) NOT NULL default '8192',"
"  skipbtaudio tinyint(1) default '0',"
"  dvb_on_demand tinyint(4) NOT NULL default '0',"
"  PRIMARY KEY  (cardid)"
");",
"CREATE TABLE IF NOT EXISTS cardinput ("
"  cardinputid int(10) unsigned NOT NULL auto_increment,"
"  cardid int(10) unsigned NOT NULL default '0',"
"  sourceid int(10) unsigned NOT NULL default '0',"
"  inputname varchar(32) NOT NULL default '',"
"  externalcommand varchar(128) default NULL,"
"  preference int(11) NOT NULL default '0',"
"  shareable char(1) default 'N',"
"  tunechan varchar(5) NOT NULL default '',"
"  startchan varchar(5) NOT NULL default '',"
"  PRIMARY KEY  (cardinputid)"
");",
"CREATE TABLE IF NOT EXISTS channel ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  channum varchar(5) NOT NULL default '',"
"  freqid varchar(10) default NULL,"
"  sourceid int(10) unsigned default NULL,"
"  callsign varchar(20) NOT NULL default '',"
"  name varchar(64) NOT NULL default '',"
"  icon varchar(255) NOT NULL default 'none',"
"  finetune int(11) default NULL,"
"  videofilters varchar(255) NOT NULL default '',"
"  xmltvid varchar(64) NOT NULL default '',"
"  recpriority int(10) NOT NULL default '0',"
"  contrast int(11) default '32768',"
"  brightness int(11) default '32768',"
"  colour int(11) default '32768',"
"  hue int(11) default '32768',"
"  tvformat varchar(10) NOT NULL default 'Default',"
"  commfree tinyint(4) NOT NULL default '0',"
"  visible tinyint(1) NOT NULL default '1',"
"  outputfilters varchar(255) NOT NULL default '',"
"  PRIMARY KEY  (chanid),"
"  KEY channel_src (channum,sourceid)"
");",
"CREATE TABLE IF NOT EXISTS codecparams ("
"  profile int(10) unsigned NOT NULL default '0',"
"  name varchar(128) NOT NULL default '',"
"  value varchar(128) default NULL,"
"  PRIMARY KEY  (profile,name)"
");",
"CREATE TABLE IF NOT EXISTS conflictresolutionany ("
"  prefertitle varchar(128) NOT NULL default '',"
"  disliketitle varchar(128) NOT NULL default '',"
"  KEY prefertitle (prefertitle),"
"  KEY disliketitle (disliketitle)"
");",
"CREATE TABLE IF NOT EXISTS conflictresolutionoverride ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime timestamp(14) NOT NULL,"
"  endtime timestamp(14) NOT NULL default '00000000000000',"
"  KEY chanid (chanid,starttime),"
"  KEY endtime (endtime)"
");",
"CREATE TABLE IF NOT EXISTS conflictresolutionsingle ("
"  preferchanid int(10) unsigned NOT NULL default '0',"
"  preferstarttime timestamp(14) NOT NULL,"
"  preferendtime timestamp(14) NOT NULL default '00000000000000',"
"  dislikechanid int(10) unsigned NOT NULL default '0',"
"  dislikestarttime timestamp(14) NOT NULL default '00000000000000',"
"  dislikeendtime timestamp(14) NOT NULL default '00000000000000',"
"  KEY preferchanid (preferchanid,preferstarttime),"
"  KEY preferendtime (preferendtime)"
");",
"CREATE TABLE IF NOT EXISTS credits ("
"  person mediumint(8) unsigned NOT NULL default '0',"
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  role set('actor','director','producer','executive_producer','writer','guest_star','host','adapter','presenter','commentator','guest') NOT NULL default '',"
"  UNIQUE KEY chanid (chanid,starttime,person,role),"
"  KEY person (person,role)"
");",
"CREATE TABLE IF NOT EXISTS dvb_channel ("
"  chanid smallint(6) NOT NULL default '0',"
"  serviceid smallint(6) default NULL,"
"  networkid smallint(6) default NULL,"
"  providerid smallint(6) default NULL,"
"  transportid smallint(6) default NULL,"
"  frequency int(11) default NULL,"
"  inversion char(1) default NULL,"
"  symbolrate int(11) default NULL,"
"  fec varchar(10) default NULL,"
"  polarity char(1) default NULL,"
"  satid smallint(6) default NULL,"
"  modulation varchar(10) default NULL,"
"  bandwidth char(1) default NULL,"
"  lp_code_rate varchar(10) default NULL,"
"  transmission_mode char(1) default NULL,"
"  guard_interval varchar(10) default NULL,"
"  hierarchy char(1) default NULL,"
"  pmtcache blob,"
"  PRIMARY KEY  (chanid)"
");",
"CREATE TABLE IF NOT EXISTS dvb_pids ("
"  chanid smallint(6) NOT NULL default '0',"
"  pid smallint(6) NOT NULL default '0',"
"  type char(1) NOT NULL default 'o',"
"  lang char(3) NOT NULL default '',"
"  PRIMARY KEY  (chanid,pid)"
");",
"CREATE TABLE IF NOT EXISTS dvb_sat ("
"  satid smallint(6) NOT NULL auto_increment,"
"  cardid int(11) default NULL,"
"  pos float default NULL,"
"  name varchar(128) default NULL,"
"  diseqc_type smallint(6) default '0',"
"  diseqc_port smallint(6) default '0',"
"  diseqc_pos smallint(6) default '0',"
"  lnb_lof_switch int(11) default '11700000',"
"  lnb_lof_hi int(11) default '10600000',"
"  lnb_lof_lo int(11) default '9750000',"
"  PRIMARY KEY  (satid)"
");",
"CREATE TABLE IF NOT EXISTS dvb_signal_quality ("
"  id int(10) unsigned NOT NULL auto_increment,"
"  sampletime timestamp(14) NOT NULL,"
"  cardid int(10) unsigned NOT NULL default '0',"
"  fe_snr int(10) unsigned NOT NULL default '0',"
"  fe_ss int(10) unsigned NOT NULL default '0',"
"  fe_ber int(10) unsigned NOT NULL default '0',"
"  fe_unc int(10) unsigned NOT NULL default '0',"
"  myth_cont int(10) unsigned NOT NULL default '0',"
"  myth_over int(10) unsigned NOT NULL default '0',"
"  myth_pkts int(10) unsigned NOT NULL default '0',"
"  PRIMARY KEY  (id),"
"  KEY sampletime (sampletime,cardid)"
");",
"CREATE TABLE IF NOT EXISTS favorites ("
"  favid int(11) unsigned NOT NULL auto_increment,"
"  userid int(11) unsigned NOT NULL default '0',"
"  chanid int(11) unsigned NOT NULL default '0',"
"  PRIMARY KEY  (favid)"
");",
"CREATE TABLE IF NOT EXISTS housekeeping ("
"  tag varchar(64) NOT NULL default '',"
"  lastrun datetime default NULL,"
"  PRIMARY KEY  (tag)"
");",
"CREATE TABLE IF NOT EXISTS jobqueue ("
"  id int(11) NOT NULL auto_increment,"
"  chanid int(10) NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  inserttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  type int(11) NOT NULL default '0',"
"  cmds int(11) NOT NULL default '0',"
"  flags int(11) NOT NULL default '0',"
"  status int(11) NOT NULL default '0',"
"  statustime timestamp(14) NOT NULL,"
"  hostname varchar(255) NOT NULL default '',"
"  args blob NOT NULL,"
"  comment varchar(128) NOT NULL default '',"
"  PRIMARY KEY  (id),"
"  UNIQUE KEY chanid (chanid,starttime,type,inserttime)"
");",
"CREATE TABLE IF NOT EXISTS jumppoints ("
"  destination varchar(128) NOT NULL default '',"
"  description varchar(255) default NULL,"
"  keylist varchar(32) default NULL,"
"  hostname varchar(255) NOT NULL default '',"
"  PRIMARY KEY  (destination,hostname)"
");",
"CREATE TABLE IF NOT EXISTS keybindings ("
"  context varchar(32) NOT NULL default '',"
"  action varchar(32) NOT NULL default '',"
"  description varchar(255) default NULL,"
"  keylist varchar(32) default NULL,"
"  hostname varchar(255) NOT NULL default '',"
"  PRIMARY KEY  (context,action,hostname)"
");",
"CREATE TABLE IF NOT EXISTS keyword ("
"  phrase varchar(128) NOT NULL default '',"
"  searchtype int(10) unsigned NOT NULL default '3',"
"  UNIQUE KEY phrase (phrase,searchtype)"
");",
"CREATE TABLE IF NOT EXISTS mythlog ("
"  logid int(10) unsigned NOT NULL auto_increment,"
"  module varchar(32) NOT NULL default '',"
"  priority int(11) NOT NULL default '0',"
"  acknowledged tinyint(1) default '0',"
"  logdate datetime default NULL,"
"  host varchar(128) default NULL,"
"  message varchar(255) NOT NULL default '',"
"  details text,"
"  PRIMARY KEY  (logid)"
");",
"CREATE TABLE IF NOT EXISTS networkiconmap ("
"  id int(11) NOT NULL auto_increment,"
"  network varchar(20) NOT NULL default '',"
"  url varchar(255) NOT NULL default '',"
"  PRIMARY KEY  (id),"
"  UNIQUE KEY network (network)"
");",
"CREATE TABLE IF NOT EXISTS oldprogram ("
"  oldtitle varchar(128) NOT NULL default '',"
"  airdate datetime NOT NULL default '0000-00-00 00:00:00',"
"  PRIMARY KEY  (oldtitle)"
");",
"CREATE TABLE IF NOT EXISTS oldrecorded ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  endtime datetime NOT NULL default '0000-00-00 00:00:00',"
"  title varchar(128) NOT NULL default '',"
"  subtitle varchar(128) NOT NULL default '',"
"  description text NOT NULL,"
"  category varchar(64) NOT NULL default '',"
"  seriesid varchar(12) NOT NULL default '',"
"  programid varchar(12) NOT NULL default '',"
"  PRIMARY KEY  (chanid,starttime),"
"  KEY endtime (endtime),"
"  KEY title (title),"
"  KEY seriesid (seriesid),"
"  KEY programid (programid)"
");",
"CREATE TABLE IF NOT EXISTS people ("
"  person mediumint(8) unsigned NOT NULL auto_increment,"
"  name char(128) NOT NULL default '',"
"  PRIMARY KEY  (person),"
"  UNIQUE KEY name (name(41))"
");",
"CREATE TABLE IF NOT EXISTS profilegroups ("
"  id int(10) unsigned NOT NULL auto_increment,"
"  name varchar(128) default NULL,"
"  cardtype varchar(32) NOT NULL default 'V4L',"
"  is_default int(1) default '0',"
"  hostname varchar(255) default NULL,"
"  PRIMARY KEY  (id),"
"  UNIQUE KEY name (name,hostname)"
");",
"CREATE TABLE IF NOT EXISTS program ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  endtime datetime NOT NULL default '0000-00-00 00:00:00',"
"  title varchar(128) NOT NULL default '',"
"  subtitle varchar(128) NOT NULL default '',"
"  description text NOT NULL,"
"  category varchar(64) NOT NULL default '',"
"  category_type varchar(64) NOT NULL default '',"
"  airdate year(4) NOT NULL default '0000',"
"  stars float NOT NULL default '0',"
"  previouslyshown tinyint(4) NOT NULL default '0',"
"  title_pronounce varchar(128) NOT NULL default '',"
"  stereo tinyint(1) default NULL,"
"  subtitled tinyint(1) default NULL,"
"  hdtv tinyint(1) default NULL,"
"  closecaptioned tinyint(1) default NULL,"
"  partnumber int(11) default NULL,"
"  parttotal int(11) default NULL,"
"  seriesid varchar(12) NOT NULL default '',"
"  originalairdate date default NULL,"
"  showtype varchar(30) NOT NULL default '',"
"  colorcode varchar(20) NOT NULL default '',"
"  syndicatedepisodenumber varchar(20) NOT NULL default '',"
"  programid varchar(12) NOT NULL default '',"
"  PRIMARY KEY  (chanid,starttime),"
"  KEY endtime (endtime),"
"  KEY title (title),"
"  KEY title_pronounce (title_pronounce),"
"  KEY seriesid (seriesid),"
"  KEY programid (programid),"
"  KEY id_start_end (chanid,starttime,endtime)"
");",
"CREATE TABLE IF NOT EXISTS programgenres ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  relevance char(1) NOT NULL default '',"
"  genre char(30) default NULL,"
"  PRIMARY KEY  (chanid,starttime,relevance)"
");",
"CREATE TABLE IF NOT EXISTS programrating ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  system char(8) NOT NULL default '',"
"  rating char(8) NOT NULL default '',"
"  UNIQUE KEY chanid (chanid,starttime,system,rating),"
"  KEY starttime (starttime,system)"
");",
"CREATE TABLE IF NOT EXISTS recgrouppassword ("
"  recgroup varchar(32) NOT NULL default '',"
"  password varchar(10) NOT NULL default '',"
"  PRIMARY KEY  (recgroup),"
"  UNIQUE KEY recgroup (recgroup)"
");",
"CREATE TABLE IF NOT EXISTS record ("
"  recordid int(10) unsigned NOT NULL auto_increment,"
"  type int(10) unsigned NOT NULL default '0',"
"  chanid int(10) unsigned default NULL,"
"  starttime time NOT NULL default '00:00:00',"
"  startdate date NOT NULL default '0000-00-00',"
"  endtime time NOT NULL default '00:00:00',"
"  enddate date NOT NULL default '0000-00-00',"
"  title varchar(128) NOT NULL default '',"
"  subtitle varchar(128) NOT NULL default '',"
"  description text NOT NULL,"
"  category varchar(64) NOT NULL default '',"
"  profile varchar(128) NOT NULL default 'Default',"
"  recpriority int(10) NOT NULL default '0',"
"  autoexpire int(11) NOT NULL default '0',"
"  maxepisodes int(11) NOT NULL default '0',"
"  maxnewest int(11) NOT NULL default '0',"
"  startoffset int(11) NOT NULL default '0',"
"  endoffset int(11) NOT NULL default '0',"
"  recgroup varchar(32) NOT NULL default 'Default',"
"  dupmethod int(11) NOT NULL default '6',"
"  dupin int(11) NOT NULL default '15',"
"  station varchar(20) NOT NULL default '',"
"  seriesid varchar(12) NOT NULL default '',"
"  programid varchar(12) NOT NULL default '',"
"  search int(10) unsigned NOT NULL default '0',"
"  autotranscode tinyint(1) NOT NULL default '0',"
"  autocommflag tinyint(1) NOT NULL default '0',"
"  autouserjob1 tinyint(1) NOT NULL default '0',"
"  autouserjob2 tinyint(1) NOT NULL default '0',"
"  autouserjob3 tinyint(1) NOT NULL default '0',"
"  autouserjob4 tinyint(1) NOT NULL default '0',"
"  PRIMARY KEY  (recordid),"
"  KEY chanid (chanid,starttime),"
"  KEY title (title),"
"  KEY seriesid (seriesid),"
"  KEY programid (programid)"
");",
"CREATE TABLE IF NOT EXISTS recorded ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  endtime datetime NOT NULL default '0000-00-00 00:00:00',"
"  title varchar(128) NOT NULL default '',"
"  subtitle varchar(128) NOT NULL default '',"
"  description text NOT NULL,"
"  category varchar(64) NOT NULL default '',"
"  hostname varchar(255) NOT NULL default '',"
"  bookmark varchar(128) default NULL,"
"  editing int(10) unsigned NOT NULL default '0',"
"  cutlist text,"
"  autoexpire int(11) NOT NULL default '0',"
"  commflagged int(10) unsigned NOT NULL default '0',"
"  recgroup varchar(32) NOT NULL default 'Default',"
"  recordid int(11) default NULL,"
"  seriesid varchar(12) NOT NULL default '',"
"  programid varchar(12) NOT NULL default '',"
"  lastmodified timestamp(14) NOT NULL,"
"  filesize bigint(20) NOT NULL default '0',"
"  stars float NOT NULL default '0',"
"  previouslyshown tinyint(1) default '0',"
"  originalairdate date default NULL,"
"  preserve tinyint(1) NOT NULL default '0',"
"  PRIMARY KEY  (chanid,starttime),"
"  KEY endtime (endtime),"
"  KEY seriesid (seriesid),"
"  KEY programid (programid)"
");",
"CREATE TABLE IF NOT EXISTS recordedmarkup ("
"  chanid int(10) unsigned NOT NULL default '0',"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  mark bigint(20) NOT NULL default '0',"
"  offset varchar(32) default NULL,"
"  type int(11) NOT NULL default '0',"
"  PRIMARY KEY  (chanid,starttime,mark,type)"
");",
"CREATE TABLE IF NOT EXISTS recordingprofiles ("
"  id int(10) unsigned NOT NULL auto_increment,"
"  name varchar(128) default NULL,"
"  videocodec varchar(128) default NULL,"
"  audiocodec varchar(128) default NULL,"
"  profilegroup int(10) unsigned NOT NULL default '0',"
"  PRIMARY KEY  (id)"
");",
"CREATE TABLE IF NOT EXISTS recordoverride ("
"  recordid int(10) unsigned NOT NULL default '0',"
"  type int(10) unsigned NOT NULL default '0',"
"  chanid int(10) unsigned default NULL,"
"  starttime datetime NOT NULL default '0000-00-00 00:00:00',"
"  endtime datetime NOT NULL default '0000-00-00 00:00:00',"
"  title varchar(128) default NULL,"
"  subtitle varchar(128) default NULL,"
"  description text,"
"  station varchar(20) NOT NULL default ''"
");",
"CREATE TABLE IF NOT EXISTS settings ("
"  value varchar(128) NOT NULL default '',"
"  data text,"
"  hostname varchar(255) default NULL,"
"  KEY value (value,hostname)"
");",
"CREATE TABLE IF NOT EXISTS videosource ("
"  sourceid int(10) unsigned NOT NULL auto_increment,"
"  name varchar(128) NOT NULL default '',"
"  xmltvgrabber varchar(128) default NULL,"
"  userid varchar(128) NOT NULL default '',"
"  freqtable varchar(16) NOT NULL default 'default',"
"  lineupid varchar(64) default NULL,"
"  password varchar(64) default NULL,"
"  PRIMARY KEY  (sourceid),"
"  UNIQUE KEY name (name)"
");",
"INSERT INTO profilegroups SET name = 'Software Encoders (v4l based)', cardtype = 'V4L', is_default = 1;",
"INSERT INTO profilegroups SET name = 'MPEG-2 Encoders (PVR-250, PVR-350)', cardtype = 'MPEG', is_default = 1;",
"INSERT INTO profilegroups SET name = 'Hardware MJPEG Encoders (Matrox G200-TV, Miro DC10, etc)', cardtype = 'MJPEG', is_default = 1;",
"INSERT INTO profilegroups SET name = 'Hardware HDTV', cardtype = 'HDTV', is_default = 1;",
"INSERT INTO profilegroups SET name = 'Hardware DVB Encoders', cardtype = 'DVB', is_default = 1;",
"INSERT INTO profilegroups SET name = 'Transcoders', cardtype = 'TRANSCODE', is_default = 1;",
"INSERT INTO recordingprofiles SET name = 'Default', profilegroup = 1;",
"INSERT INTO recordingprofiles SET name = 'Live TV', profilegroup = 1;",
"INSERT INTO recordingprofiles SET name = 'High Quality', profilegroup = 1;",
"INSERT INTO recordingprofiles SET name = 'Low Quality', profilegroup = 1;",
"INSERT INTO recordingprofiles SET name = 'Default', profilegroup = 2;",
"INSERT INTO recordingprofiles SET name = 'Live TV', profilegroup = 2;",
"INSERT INTO recordingprofiles SET name = 'High Quality', profilegroup = 2;",
"INSERT INTO recordingprofiles SET name = 'Low Quality', profilegroup = 2;",
"INSERT INTO recordingprofiles SET name = 'Default', profilegroup = 3;",
"INSERT INTO recordingprofiles SET name = 'Live TV', profilegroup = 3;",
"INSERT INTO recordingprofiles SET name = 'High Quality', profilegroup = 3;",
"INSERT INTO recordingprofiles SET name = 'Low Quality', profilegroup = 3;",
"INSERT INTO recordingprofiles SET name = 'Default', profilegroup = 4;",
"INSERT INTO recordingprofiles SET name = 'Live TV', profilegroup = 4;",
"INSERT INTO recordingprofiles SET name = 'High Quality', profilegroup = 4;",
"INSERT INTO recordingprofiles SET name = 'Low Quality', profilegroup = 4;",
"INSERT INTO recordingprofiles SET name = 'Default', profilegroup = 5;",
"INSERT INTO recordingprofiles SET name = 'Live TV', profilegroup = 5;",
"INSERT INTO recordingprofiles SET name = 'High Quality', profilegroup = 5;",
"INSERT INTO recordingprofiles SET name = 'Low Quality', profilegroup = 5;",
"INSERT INTO recordingprofiles SET name = 'RTjpeg/MPEG4', profilegroup = 6;",
"INSERT INTO recordingprofiles SET name = 'MPEG2', profilegroup = 6;",
"INSERT INTO settings SET value='mythfilldatabaseLastRunStart';",
"INSERT INTO settings SET value='mythfilldatabaseLastRunEnd';",
"INSERT INTO settings SET value='mythfilldatabaseLastRunStatus';",
"INSERT INTO settings SET value='DataDirectMessage';",
"INSERT INTO settings SET value='HaveRepeats', data='0';",
"INSERT INTO settings SET value='DBSchemaVer', data='1060';",
""
};

    QString dbver = "";
    if (!performActualUpdate(updates, "1060", dbver))
        return false;
    return true;
}

