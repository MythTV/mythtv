USE mythconverg;
CREATE TABLE musicmetadata
(
    artist VARCHAR(128) NOT NULL,
    album VARCHAR(128) NOT NULL,
    title VARCHAR(128) NOT NULL,
    genre VARCHAR(128) NOT NULL,
    year INT UNSIGNED NOT NULL,
    tracknum INT UNSIGNED NOT NULL,
    length INT UNSIGNED NOT NULL,
    filename TEXT NOT NULL,
    INDEX (artist),
    INDEX (album),
    INDEX (title),
    INDEX (genre)
); 
