USE mythconverg;

DELETE FROM settings WHERE value='DBSchemaVer';
INSERT INTO settings VALUES ('DBSchemaVer', 1003, NULL);

#
#   Below are the changes to the database between MythTV
#   versions 0.10 & 0.11.
#

DELETE FROM settings WHERE value="MaxTranscoders";
UPDATE transcoding SET status = 1, starttime = starttime;

ALTER TABLE record ADD recorddups INT DEFAULT 0 NOT NULL;
ALTER TABLE record ADD maxnewest INT DEFAULT 0 NOT NULL;

INSERT INTO settings SET value="TranscoderAutoRun", data=0;
DELETE FROM settings WHERE value="TranscoderUseCutlist";

ALTER TABLE record ADD maxepisodes INT DEFAULT 0 NOT NULL;

ALTER TABLE record ADD autoexpire INT DEFAULT 0 NOT NULL;
ALTER TABLE recorded ADD autoexpire INT DEFAULT 0 NOT NULL;

