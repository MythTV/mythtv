USE mythconverg;
CREATE TABLE musicmetadata
(
    intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    artist VARCHAR(128) NOT NULL,
    album VARCHAR(128) NOT NULL,
    title VARCHAR(128) NOT NULL,
    genre VARCHAR(128) NOT NULL,
    year INT UNSIGNED NOT NULL,
    tracknum INT UNSIGNED NOT NULL,
    length INT UNSIGNED NOT NULL,
    filename TEXT NOT NULL,
    rating INT UNSIGNED NOT NULL DEFAULT 5,
    lastplay TIMESTAMP NOT NULL,
    playcount INT UNSIGNED NOT NULL DEFAULT 0,
    INDEX (artist),
    INDEX (album),
    INDEX (title),
    INDEX (genre)
);
CREATE TABLE musicplaylist
(
    playlistid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    songlist TEXT NOT NULL
);
 
