USE mythconverg;

ALTER TABLE videometadata ADD playcommand VARCHAR(255);
ALTER TABLE videometadata ADD INDEX(title);
ALTER TABLE videometadata ADD browse BOOL NOT NULL DEFAULT 1;

CREATE TABLE videotypes
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
