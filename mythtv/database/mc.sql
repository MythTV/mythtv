CREATE DATABASE mythconverg;
GRANT ALL ON mythconverg.* TO mythtv@localhost IDENTIFIED BY "mythtv";

USE mythconverg;

#
#   The version number of the database schema is set by the
#   first INSERT command after all tables have been created.
#

CREATE TABLE IF NOT EXISTS recordingprofiles
(
    id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    name VARCHAR(128),
    videocodec VARCHAR(128),
    audiocodec VARCHAR(128),
    UNIQUE(name)
);
CREATE TABLE IF NOT EXISTS codecparams
(
    profile INT UNSIGNED NOT NULL,
    name VARCHAR(128) NOT NULL,
    value VARCHAR(128),
    PRIMARY KEY (profile, name)
);
CREATE TABLE IF NOT EXISTS channel
(
    chanid INT UNSIGNED NOT NULL PRIMARY KEY,
    channum VARCHAR(5) NOT NULL,
    freqid VARCHAR(5) NOT NULL,
    sourceid INT UNSIGNED,
    callsign VARCHAR(20) NULL,
    name VARCHAR(20) NULL,
    icon VARCHAR(255) NULL,
    finetune INT,
    videofilters VARCHAR(255) NULL,
    xmltvid VARCHAR(64) NULL,
    rank INT(10) DEFAULT '0' NOT NULL,
    contrast INT DEFAULT 32768,
    brightness INT DEFAULT 32768,
    colour INT DEFAULT 32768,
    hue INT DEFAULT 32768
);
CREATE TABLE IF NOT EXISTS channel_dvb
(
    chanid INT UNSIGNED NOT NULL PRIMARY KEY,
    listingid VARCHAR(20) NULL,
    pids VARCHAR(50),
    freq INT UNSIGNED,
    pol CHAR DEFAULT 'V',
    symbol_rate INT UNSIGNED NULL,
    tone INT UNSIGNED NULL,
    diseqc INT UNSIGNED NULL,
    inversion VARCHAR(10) NULL,
    bandwidth VARCHAR(10) NULL,
    hp_code_rate VARCHAR(10) NULL,
    lp_code_rate VARCHAR(10) NULL,
    modulation VARCHAR(10) NULL,
    transmission_mode VARCHAR(10) NULL,
    guard_interval VARCHAR(10) NULL,
    hierarchy VARCHAR(10) NULL
);
CREATE TABLE IF NOT EXISTS program
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    category VARCHAR(64) NULL,
    category_type VARCHAR(64) NULL,
    airdate YEAR NOT NULL,
    stars FLOAT UNSIGNED NOT NULL,
    previouslyshown TINYINT NOT NULL default '0',
    PRIMARY KEY (chanid, starttime),
    INDEX (endtime),
    INDEX (title)
);
CREATE TABLE IF NOT EXISTS record
(
    recordid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    type INT UNSIGNED NOT NULL,
    chanid INT UNSIGNED NULL,
    starttime TIME NOT NULL,
    startdate DATE NOT NULL,
    endtime TIME NOT NULL,
    enddate DATE NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    category VARCHAR(64) NULL,
    profile INT UNSIGNED NOT NULL DEFAULT 0,
    rank INT(10) DEFAULT '0' NOT NULL,
    INDEX (chanid, starttime),
    INDEX (title)
);
CREATE TABLE IF NOT EXISTS recorded
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    category VARCHAR(64) NULL,
    hostname VARCHAR(255),
    bookmark VARCHAR(128) NULL,
    editing INT UNSIGNED NOT NULL DEFAULT 0,
    cutlist TEXT NULL,
    PRIMARY KEY (chanid, starttime),
    INDEX (endtime)
);
CREATE TABLE IF NOT EXISTS settings
(
    value VARCHAR(128) NOT NULL,
    data TEXT NULL,
    hostname VARCHAR(255) NULL,
    INDEX (value, hostname)
);
CREATE TABLE IF NOT EXISTS conflictresolutionoverride
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    INDEX (chanid, starttime),
    INDEX (endtime)
);
CREATE TABLE IF NOT EXISTS conflictresolutionsingle
(
    preferchanid INT UNSIGNED NOT NULL,
    preferstarttime TIMESTAMP NOT NULL,
    preferendtime TIMESTAMP NOT NULL,
    dislikechanid INT UNSIGNED NOT NULL,
    dislikestarttime TIMESTAMP NOT NULL,
    dislikeendtime TIMESTAMP NOT NULL,
    INDEX (preferchanid, preferstarttime),
    INDEX (preferendtime)
);
CREATE TABLE IF NOT EXISTS conflictresolutionany
(
    prefertitle VARCHAR(128) NOT NULL,
    disliketitle VARCHAR(128) NOT NULL,
    INDEX (prefertitle),
    INDEX (disliketitle)
);
CREATE TABLE IF NOT EXISTS oldrecorded
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    category VARCHAR(64) NULL,
    PRIMARY KEY (chanid, starttime),
    INDEX (endtime),
    INDEX (title)
);
CREATE TABLE IF NOT EXISTS capturecard
(
    cardid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    videodevice VARCHAR(128),
    audiodevice VARCHAR(128),
    vbidevice VARCHAR(128),
    cardtype VARCHAR(32) DEFAULT 'V4L',
    defaultinput VARCHAR(32) DEFAULT 'Television',
    audioratelimit INT,
    hostname VARCHAR(255),
    use_ts INT NULL,
    dvb_type CHAR NULL
);
CREATE TABLE IF NOT EXISTS videosource
(
    sourceid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    name VARCHAR(128),
    xmltvgrabber VARCHAR(128),
    userid VARCHAR(128) NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS cardinput
(
    cardinputid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    cardid INT UNSIGNED NOT NULL,
    sourceid INT UNSIGNED NOT NULL,
    inputname VARCHAR(32) NOT NULL,
    externalcommand VARCHAR(128) NULL,
    preference INT,
    shareable CHAR DEFAULT 'N',
    tunechan CHAR(5) NOT NULL,
    startchan CHAR(5) NOT NULL
);
CREATE TABLE IF NOT EXISTS favorites (
    favid int(11) unsigned NOT NULL auto_increment,
    userid int(11) unsigned NOT NULL default '0',
    chanid int(11) unsigned NOT NULL default '0',
    PRIMARY KEY (favid)
);

CREATE TABLE IF NOT EXISTS recordedmarkup
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    mark BIGINT(20) NOT NULL,
    offset VARCHAR(32) NULL,
    type INT NOT NULL,
    primary key (chanid,starttime, mark, type )
);
CREATE TABLE IF NOT EXISTS programrating
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    system CHAR(8) NOT NULL default '',
    rating CHAR(8) NOT NULL default '',
    UNIQUE KEY chanid (chanid,starttime,system,rating),
    INDEX (starttime, system)
);
CREATE TABLE IF NOT EXISTS people
(
    person MEDIUMINT(8) UNSIGNED NOT NULL AUTO_INCREMENT,
    name CHAR(128) NOT NULL default '',
    PRIMARY KEY (person),
    KEY name (name(20))
) TYPE=MyISAM;

CREATE TABLE IF NOT EXISTS credits
(
    person MEDIUMINT(8) UNSIGNED NOT NULL default '0',
    chanid INT UNSIGNED NOT NULL default '0',
    starttime TIMESTAMP NOT NULL,
    role SET('actor','director','producer','executive_producer','writer','guest_star','host','adapter','presenter','commentator','guest') NOT NULL default '',
    UNIQUE KEY chanid (chanid, starttime, person),
    KEY person (person, role)
) TYPE=MyISAM;


CREATE TABLE IF NOT EXISTS transcoding (
    chanid INT UNSIGNED,
    starttime TIMESTAMP,
    status INT,
    hostname VARCHAR(255)
);

INSERT INTO settings VALUES ('DBSchemaVer', 1000, NULL);

INSERT INTO recordingprofiles (name) VALUES ('Default');
INSERT INTO recordingprofiles (name) VALUES ('Live TV');
INSERT INTO recordingprofiles (name) VALUES ('Transcode');
