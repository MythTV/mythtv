USE mythconverg;

CREATE TABLE IF NOT EXISTS people
(
    person MEDIUMINT(8) UNSIGNED NOT NULL AUTO_INCREMENT,
    name CHAR(128) NOT NULL default '',
    PRIMARY KEY (person),
    KEY name (name(20))
) TYPE=MyISAM;

CREATE TABLE IF NOT EXISTS credits
(
    person MEDIUMINT(8) UNSIGNED NOT NULL default '0',
    chanid INT UNSIGNED NOT NULL default '0',
    starttime TIMESTAMP NOT NULL,
    role SET('actor','director','producer','executive_producer','writer','guest_star','host','adapter','presenter','commentator','guest') NOT NULL default '',
    UNIQUE KEY chanid (chanid, starttime, person),
    KEY person (person, role)
) TYPE=MyISAM;

CREATE TABLE IF NOT EXISTS transcoding (
    chanid INT NOT NULL,
    starttime TIMESTAMP,
    isdone INT UNSIGNED NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS channel_dvb
(
    chanid INT UNSIGNED NOT NULL PRIMARY KEY,
    listingid VARCHAR(20) NULL,
    pids VARCHAR(50),
    freq INT UNSIGNED,
    pol CHAR DEFAULT 'V',
    symbol_rate INT UNSIGNED NULL,
    tone INT UNSIGNED NULL,
    diseqc INT UNSIGNED NULL,
    inversion VARCHAR(10) NULL,
    bandwidth VARCHAR(10) NULL,
    hp_code_rate VARCHAR(10) NULL,
    lp_code_rate VARCHAR(10) NULL,
    modulation VARCHAR(10) NULL,
    transmission_mode VARCHAR(10) NULL,
    guard_interval VARCHAR(10) NULL,
    hierarchy VARCHAR(10) NULL
);

ALTER TABLE program ADD COLUMN previouslyshown TINYINT NOT NULL default '0';

ALTER TABLE videosource ADD COLUMN userid VARCHAR(128) NOT NULL default '';

CREATE INDEX progid ON record (chanid, starttime);
CREATE INDEX title ON record (title(10)); 
CREATE INDEX title ON program (title(10));

INSERT INTO recordingprofiles (name) VALUES ('Transcode');

ALTER TABLE recordedmarkup ADD COLUMN offset VARCHAR(32) NULL; 

ALTER TABLE capturecard ADD COLUMN use_ts INT NULL;
ALTER TABLE capturecard ADD COLUMN dvb_type CHAR NULL;
  
