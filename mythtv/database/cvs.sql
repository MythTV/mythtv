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

ALTER TABLE record ADD maxepisodes INT DEFAULT 0 NOT NULL;

ALTER TABLE record ADD autoexpire INT DEFAULT 0 NOT NULL;
ALTER TABLE recorded ADD autoexpire INT DEFAULT 0 NOT NULL;

