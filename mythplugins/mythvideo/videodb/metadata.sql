USE mythconverg;
CREATE TABLE videometadata
(
    intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    title VARCHAR(128) NOT NULL,
    director VARCHAR(128) NOT NULL,
    plot VARCHAR(255) NOT NULL,
    rating VARCHAR(128) NOT NULL,
    inetref VARCHAR(32) NOT NULL,
    year INT UNSIGNED NOT NULL,
    userrating FLOAT NOT NULL,
    length INT UNSIGNED NOT NULL,
    showlevel INT UNSIGNED NOT NULL,
    filename TEXT NOT NULL,
    coverfile TEXT NOT NULL,
    childid INT UNSIGNED NOT NULL DEFAULT 0,
    INDEX (director)
);
 
