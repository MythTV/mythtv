USE mythconverg;
CREATE TABLE singlerecord
(
    channum INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    PRIMARY KEY (channum, starttime),
    INDEX (endtime)
);
CREATE TABLE timeslotrecord
(
    channum INT UNSIGNED NOT NULL,
    starttime TIME NOT NULL,
    endtime TIME NOT NULL,
    title VARCHAR(128) NULL,
    PRIMARY KEY(channum, starttime),
    INDEX (endtime)
);
CREATE TABLE allrecord
(
    title VARCHAR(128) NULL
);
CREATE TABLE recorded
(
    channum INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    PRIMARY KEY (channum, starttime),
    INDEX (endtime)
);

