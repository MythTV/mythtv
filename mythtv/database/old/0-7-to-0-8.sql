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

INSERT INTO record
(type,chanid,starttime,startdate,endtime,enddate,title,subtitle,description,profile)
SELECT 1, chanid,
SEC_TO_TIME(TIME_TO_SEC(starttime)), FROM_DAYS(TO_DAYS(starttime)),
SEC_TO_TIME(TIME_TO_SEC(endtime)), FROM_DAYS(TO_DAYS(endtime)),
title,subtitle,description,profile
FROM singlerecord;

INSERT INTO record
(type,chanid,starttime,endtime,title,profile)
SELECT 2, chanid, starttime, endtime, title, profile
FROM timeslotrecord;

INSERT INTO record
(type,chanid,title,profile)
SELECT 4, chanid, title, profile
FROM allrecord
WHERE chanid IS NOT NULL;

INSERT INTO record
(type,chanid,title,profile)
SELECT 3, chanid, title, profile
FROM allrecord
WHERE chanid IS NULL;

ALTER TABLE settings ADD COLUMN hostname VARCHAR(255) NULL;

ALTER TABLE capturecard ADD COLUMN hostname VARCHAR(255);
ALTER TABLE recorded ADD COLUMN hostname VARCHAR(255);

ALTER TABLE allrecord ADD COLUMN profile INT UNSIGNED NOT NULL DEFAULT 0 REFERENCES recordingprofiles(id);
ALTER TABLE singlerecord ADD COLUMN profile INT UNSIGNED NOT NULL DEFAULT 0 REFERENCES recordingprofiles(id);
ALTER TABLE timeslotrecord ADD COLUMN profile INT UNSIGNED NOT NULL DEFAULT 0 REFERENCES recordingprofiles(id);
ALTER TABLE videosource ADD COLUMN xmltvgrabber VARCHAR(128);

UPDATE allrecord SET profile = 0;
UPDATE singlerecord SET profile = 0;
UPDATE timeslotrecord SET profile = 0;

INSERT INTO recordingprofiles (name) VALUES ('Default');
INSERT INTO recordingprofiles (name) VALUES ('Live TV');
