CREATE DATABASE mythconverg;
GRANT ALL ON mythconverg.* TO mythtv@localhost IDENTIFIED BY "mythtv";
USE mythconverg;
CREATE TABLE channel
(
    chanid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    channum VARCHAR(5) NOT NULL,
    sourceid INT UNSIGNED;
    callsign VARCHAR(20) NULL,
    name VARCHAR(20) NULL,
    icon VARCHAR(255) NULL,
    finetune INT,
    videofilters VARCHAR(255) NULL
);
CREATE TABLE program
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
CREATE TABLE singlerecord
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
CREATE TABLE timeslotrecord
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIME NOT NULL,
    endtime TIME NOT NULL,
    title VARCHAR(128) NULL,
    PRIMARY KEY(chanid, starttime),
    INDEX (endtime)
);
CREATE TABLE allrecord
(
    title VARCHAR(128) NULL
);
CREATE TABLE recorded
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
CREATE TABLE settings
(
    value VARCHAR(128) NOT NULL,
    data TEXT NULL,
    PRIMARY KEY(value)
);
CREATE TABLE conflictresolutionoverride
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    INDEX (chanid, starttime),
    INDEX (endtime)
);
CREATE TABLE conflictresolutionsingle
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
CREATE TABLE conflictresolutionany
(
    prefertitle VARCHAR(128) NOT NULL,
    disliketitle VARCHAR(128) NOT NULL,
    INDEX (prefertitle),
    INDEX (disliketitle)
);
CREATE TABLE oldrecorded
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
CREATE TABLE capturecard
(
    cardid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    videodevice VARCHAR(128),
    audiodevice VARCHAR(128),
    cardtype VARCHAR(32) DEFAULT 'V4L',
    audioratelimit INT
);
CREATE TABLE videosource
(
    sourceid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    name VARCHAR(128)
);
CREATE TABLE cardinput
(
    cardinputid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    cardid INT UNSIGNED NOT NULL,
    sourceid INT UNSIGNED NOT NULL,
    inputname VARCHAR(32) NOT NULL,
    externalcommand VARCHAR(128) NULL,
    preference INT,
    shareable CHAR DEFAULT 'N'
);
 
