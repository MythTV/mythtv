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
    profile INT UNSIGNED NOT NULL REFERENCES recordingprofiles(id),
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
    PRIMARY KEY (chanid, starttime),
    INDEX (endtime)
);
CREATE TABLE IF NOT EXISTS singlerecord
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    profile INT UNSIGNED NOT NULL DEFAULT 0 REFERENCES recordingprofiles(id),
    PRIMARY KEY (chanid, starttime),
    INDEX (endtime)
);
CREATE TABLE IF NOT EXISTS timeslotrecord
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIME NOT NULL,
    endtime TIME NOT NULL,
    title VARCHAR(128) NULL,
    profile INT UNSIGNED NOT NULL DEFAULT 0 REFERENCES recordingprofiles(id),
    PRIMARY KEY(chanid, starttime),
    INDEX (endtime)
);
CREATE TABLE IF NOT EXISTS allrecord
(
    title VARCHAR(128) NULL,
    chanid INT UNSIGNED NULL,
    profile INT UNSIGNED NOT NULL DEFAULT 0 REFERENCES recordingprofiles(id)
);
CREATE TABLE IF NOT EXISTS recorded
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    PRIMARY KEY (chanid, starttime),
    INDEX (endtime)
);
CREATE TABLE IF NOT EXISTS settings
(
    value VARCHAR(128) NOT NULL,
    data TEXT NULL,
    PRIMARY KEY(value)
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
    cardtype VARCHAR(32) DEFAULT 'V4L',
    audioratelimit INT
);
CREATE TABLE IF NOT EXISTS videosource
(
    sourceid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    name VARCHAR(128)
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

INSERT INTO recordingprofiles (name) VALUES ('Default');
INSERT INTO recordingprofiles (name) VALUES ('Live TV');

