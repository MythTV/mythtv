USE mythconverg;
CREATE TABLE IF NOT EXISTS videometadata
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
    browse BOOL NOT NULL DEFAULT 1,
    playcommand VARCHAR(255),
    INDEX (director),
    INDEX (title)
);
 
CREATE TABLE IF NOT EXISTS videotypes
(
    intid       INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    extension   VARCHAR(128) NOT NULL,
    playcommand VARCHAR(255) NOT NULL,
    f_ignore    BOOL,
    use_default BOOL    
);

#
#   pre-defined values
#

INSERT INTO videotypes
    (extension, playcommand, f_ignore, use_default)
    VALUES
    ("txt", "", 1, 0);

INSERT INTO videotypes
    (extension, playcommand, f_ignore, use_default)
    VALUES
    ("log", "", 1, 0);

INSERT INTO videotypes
    (extension, playcommand, f_ignore, use_default)
    VALUES
    ("mpg", "", 0, 1);

INSERT INTO videotypes
    (extension, playcommand, f_ignore, use_default)
    VALUES
    ("avi", "", 0, 1);

INSERT INTO videotypes
    (extension, playcommand, f_ignore, use_default)
    VALUES
    ("vob", "", 0, 1);

INSERT INTO videotypes
    (extension, playcommand, f_ignore, use_default)
    VALUES
    ("mpeg", "", 0, 1);

