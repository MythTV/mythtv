USE mythconverg;

DELETE FROM settings WHERE value='DBSchemaVer';
INSERT INTO settings VALUES ('DBSchemaVer', 901, NULL);

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

