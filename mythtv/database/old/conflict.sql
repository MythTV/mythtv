USE mythconverg;
CREATE TABLE oldrecorded
(
    channum INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    INDEX (channum, starttime),
    INDEX (endtime),
    INDEX (title)
);
CREATE TABLE conflictresolutionoverride
(
    channum INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL, 
    endtime TIMESTAMP NOT NULL,
    INDEX (channum, starttime),
    INDEX (endtime)
);
CREATE TABLE conflictresolutionsingle
(
    preferchannum INT UNSIGNED NOT NULL,
    preferstarttime TIMESTAMP NOT NULL,
    preferendtime TIMESTAMP NOT NULL,
    dislikechannum INT UNSIGNED NOT NULL,
    dislikestarttime TIMESTAMP NOT NULL,
    dislikeendtime TIMESTAMP NOT NULL,
    PRIMARY KEY (preferchannum, preferstarttime),
    INDEX (preferendtime)
);
CREATE TABLE conflictresolutionany
(
    prefertitle VARCHAR(128) NOT NULL,
    disliketitle VARCHAR(128) NOT NULL,
    INDEX (prefertitle),
    INDEX (disliketitle)
);
