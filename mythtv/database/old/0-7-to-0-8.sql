CREATE TABLE IF NOT EXISTS recordingprofiles
(
    id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    videocodec VARCHAR(128) NOT NULL,
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

ALTER TABLE allrecord ADD COLUMN profile INT UNSIGNED NOT NULL DEFAULT 1 REFERENCES recordingprofile(id);
ALTER TABLE singlerecord ADD COLUMN profile INT UNSIGNED NOT NULL DEFAULT 1 REFERENCES recordingprofile(id);
ALTER TABLE timeslotrecord ADD COLUMN profile INT UNSIGNED NOT NULL DEFAULT 1 REFERENCES recordingprofile(id);

UPDATE allrecord SET profile = 1;
UPDATE singlerecord SET profile = 1;
UPDATE timeslotrecord SET profile = 1;

