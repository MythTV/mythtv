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

ALTER TABLE program ADD COLUMN previouslyshown TINYINT NOT NULL default '0';

ALTER TABLE videosource ADD COLUMN userid VARCHAR(128) NOT NULL default '';

CREATE INDEX progid ON record (chanid, starttime);
CREATE INDEX title ON record (title(10)); 
CREATE INDEX title ON program (title(10));   
