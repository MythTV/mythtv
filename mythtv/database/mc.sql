CREATE DATABASE mythconverg;
GRANT ALL ON mythconverg.* TO mythtv@localhost IDENTIFIED BY "mythtv";

USE mythconverg;

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
    sourceid INT UNSIGNED,
    callsign VARCHAR(20) NULL,
    name VARCHAR(20) NULL,
    icon VARCHAR(255) NULL,
    finetune INT,
    videofilters VARCHAR(255) NULL,
    xmltvid VARCHAR(64) NULL
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
    airdate YEAR NOT NULL,
    stars FLOAT UNSIGNED NOT NULL,
    PRIMARY KEY (chanid, starttime),
    INDEX (endtime)
);
CREATE TABLE IF NOT EXISTS record
(
    recordid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    type INT UNSIGNED NOT NULL,
    chanid INT UNSIGNED NULL,
    starttime TIME NULL,
    startdate DATE NULL,
    endtime TIME NULL,
    enddate DATE NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    profile INT UNSIGNED NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS recorded
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
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
    hostname VARCHAR(255)
);
CREATE TABLE IF NOT EXISTS videosource
(
    sourceid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    name VARCHAR(128),
    xmltvgrabber VARCHAR(128)
);
CREATE TABLE IF NOT EXISTS cardinput
(
    cardinputid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    cardid INT UNSIGNED NOT NULL,
    sourceid INT UNSIGNED NOT NULL,
    inputname VARCHAR(32) NOT NULL,
    externalcommand VARCHAR(128) NULL,
    preference INT,
    shareable CHAR DEFAULT 'N'
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

INSERT INTO recordingprofiles (name) VALUES ('Default');
INSERT INTO recordingprofiles (name) VALUES ('Live TV');

