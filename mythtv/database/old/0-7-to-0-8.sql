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

ALTER TABLE allrecord ADD COLUMN profile INT UNSIGNED NOT NULL DEFAULT 0 REFERENCES recordingprofile(id);
ALTER TABLE singlerecord ADD COLUMN profile INT UNSIGNED NOT NULL DEFAULT 0 REFERENCES recordingprofile(id);
ALTER TABLE timeslotrecord ADD COLUMN profile INT UNSIGNED NOT NULL DEFAULT 0 REFERENCES recordingprofile(id);

UPDATE allrecord SET profile = 0;
UPDATE singlerecord SET profile = 0;
UPDATE timeslotrecord SET profile = 0;

INSERT INTO recordingprofiles (name) VALUES ('Default');
INSERT INTO recordingprofiles (name) VALUES ('Live TV');
