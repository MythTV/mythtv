USE mythconverg;

CREATE TABLE IF NOT EXISTS favorites (
    favid int(11) unsigned NOT NULL auto_increment,
    userid int(11) unsigned NOT NULL default '0',
    chanid int(11) unsigned NOT NULL default '0',
    PRIMARY KEY (favid)
);

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

CREATE TABLE IF NOT EXISTS markup
(
    chanid INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    mark BIGINT(20) NOT NULL,
    type INT NOT NULL,
    primary key (chanid,starttime, mark, type )
);  

INSERT INTO record
(type,chanid,starttime,startdate,endtime,enddate,title,subtitle,description)
SELECT 1, chanid,
SEC_TO_TIME(TIME_TO_SEC(starttime)), FROM_DAYS(TO_DAYS(starttime)),
SEC_TO_TIME(TIME_TO_SEC(endtime)), FROM_DAYS(TO_DAYS(endtime)),
title,subtitle,description
FROM singlerecord;

INSERT INTO record
(type,chanid,starttime,endtime,title)
SELECT 2, chanid, starttime, endtime, title
FROM timeslotrecord;

INSERT INTO record
(type,chanid,title)
SELECT 3, chanid, title
FROM allrecord
WHERE chanid IS NOT NULL;

INSERT INTO record
(type,chanid,title)
SELECT 4, chanid, title
FROM allrecord
WHERE chanid IS NULL;

ALTER TABLE settings ADD COLUMN hostname VARCHAR(255) NULL;
ALTER TABLE settings DROP PRIMARY KEY;
ALTER TABLE settings ADD INDEX(value, hostname);

ALTER TABLE capturecard ADD COLUMN hostname VARCHAR(255);
ALTER TABLE capturecard ADD COLUMN vbidevice VARCHAR(255);
ALTER TABLE capturecard ADD COLUMN defaultinput VARCHAR(32) DEFAULT 'Television';

ALTER TABLE recorded ADD COLUMN bookmark VARCHAR(128) NULL;
ALTER TABLE recorded ADD COLUMN editing INT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE recorded ADD COLUMN cutlist TEXT NULL;
ALTER TABLE recorded ADD COLUMN hostname VARCHAR(255);

ALTER TABLE videosource ADD COLUMN xmltvgrabber VARCHAR(128);

INSERT INTO recordingprofiles (name) VALUES ('Default');
INSERT INTO recordingprofiles (name) VALUES ('Live TV');
