USE mythconverg;

DELETE FROM settings WHERE value='DBSchemaVer';
INSERT INTO settings VALUES ('DBSchemaVer', 1000, NULL);

# The transcoding table now contains different information
# than it originally did.  It is not safe to keep any existing
# rows.

DROP TABLE IF EXISTS transcoding;
CREATE TABLE transcoding (chanid INT UNSIGNED, starttime TIMESTAMP,
                          status INT, hostname VARCHAR(255));
INSERT INTO settings VALUES ('TranscoderUseCutlist', '0', NULL);


ALTER TABLE program ADD COLUMN category_type VARCHAR(64) NULL;

ALTER TABLE record ADD COLUMN category VARCHAR(64) NULL;
ALTER TABLE recorded ADD COLUMN category VARCHAR(64) NULL;
ALTER TABLE oldrecorded ADD COLUMN category VARCHAR(64) NULL;

ALTER TABLE record ADD rank INT(10) DEFAULT '0' NOT NULL;
ALTER TABLE channel ADD rank INT(10) DEFAULT '0' NOT NULL;

ALTER TABLE channel ADD COLUMN freqid VARCHAR(5) NOT NULL;
UPDATE channel set freqid=channum;

