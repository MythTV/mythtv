USE mythconverg;
CREATE TABLE conflictresolutionoverride
(
    channum INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL, 
    endtime TIMESTAMP NOT NULL,
    PRIMARY KEY (channum, starttime),
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
