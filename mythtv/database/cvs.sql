USE mythconverg;

ALTER TABLE settings DROP PRIMARY KEY;
ALTER TABLE settings ADD INDEX(value, hostname);
ALTER TABLE capturecard ADD COLUMN defaultinput VARCHAR(32) DEFAULT 'Television';

ALTER TABLE recorded ADD COLUMN bookmark VARCHAR(128) NULL;
ALTER TABLE recorded ADD COLUMN editing INT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE recorded ADD COLUMN cutlist TEXT NULL;
ALTER TABLE capturecard ADD COLUMN vbidevice VARCHAR(255);
REPLACE INTO settings (value, data) VALUES ("LCDHost","localhost");
REPLACE INTO settings (value, data) VALUES ("LCDPort","13666");
