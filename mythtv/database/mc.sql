CREATE DATABASE mythconverg;
GRANT ALL ON mythconverg.* TO mythtv@localhost IDENTIFIED BY "mythtv";
USE mythconverg;
CREATE TABLE channel
(
    channum INT UNSIGNED NOT NULL PRIMARY KEY,
    callsign VARCHAR(20) NULL,
    name VARCHAR(20) NULL,
    icon VARCHAR(255) NULL
);
CREATE TABLE program
(
    channum INT UNSIGNED NOT NULL,
    starttime TIMESTAMP NOT NULL,
    endtime TIMESTAMP NOT NULL,
    title VARCHAR(128) NULL,
    subtitle VARCHAR(128) NULL,
    description TEXT NULL,
    category VARCHAR(64) NULL
);
     
