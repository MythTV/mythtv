USE mythconverg;

#
#   Set the version number of the database schema.
#   This is compared to the version number in mythcontext.h
#   to verify that database changes are up to date.
#

DELETE FROM settings WHERE value='DBSchemaVer';
INSERT INTO settings VALUES ('DBSchemaVer', 1002, NULL);

#
#   Below are the recent alterations to the database.
#   The most recent are listed first. Execution should fail
#   when a previously executed command is encountered.
#

INSERT INTO settings SET value="TranscoderAutoRun", data=0;
DELETE FROM settings WHERE value="TranscoderUseCutlist";
INSERT INTO settings SET value="TranscoderUseCutlist", data=1;

ALTER TABLE record ADD maxepisodes INT DEFAULT 0 NOT NULL;

ALTER TABLE record ADD autoexpire INT DEFAULT 0 NOT NULL;
ALTER TABLE recorded ADD autoexpire INT DEFAULT 0 NOT NULL;

