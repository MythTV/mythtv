USE mythconverg;
DELETE FROM channel;
DELETE FROM program;
DELETE FROM singlerecord;
DELETE FROM timeslotrecord;
DELETE FROM conflictresolutionoverride;
DELETE FROM conflictresolutionsingle;
DELETE FROM conflictresolutionany;

ALTER TABLE channel CHANGE channum channum VARCHAR(5) NOT NULL;
ALTER TABLE channel DROP PRIMARY KEY;
ALTER TABLE channel ADD chanid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY;
ALTER TABLE channel ADD sourceid INT UNSIGNED;
ALTER TABLE channel ADD finetune INT;
ALTER TABLE channel ADD videofilters VARCHAR(255) NULL;

ALTER TABLE program CHANGE channum chanid INT UNSIGNED NOT NULL;
ALTER TABLE singlerecord CHANGE channum chanid INT UNSIGNED NOT NULL;
ALTER TABLE timeslotrecord CHANGE channum chanid INT UNSIGNED NOT NULL;
ALTER TABLE recorded CHANGE channum chanid INT UNSIGNED NOT NULL;
ALTER TABLE conflictresolutionoverride CHANGE channum chanid INT UNSIGNED NOT NULL;
ALTER TABLE conflictresolutionsingle CHANGE preferchannum preferchanid INT UNSIGNED NOT NULL;
ALTER TABLE conflictresolutionsingle CHANGE dislikechannum dislikechanid INT UNSIGNED NOT NULL;
ALTER TABLE oldrecorded CHANGE channum chanid INT UNSIGNED NOT NULL;

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
