USE mythconverg;

ALTER TABLE recorded ADD COLUMN bookmark VARCHAR(128) NULL;
ALTER TABLE recorded ADD COLUMN editing INT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE recorded ADD COLUMN cutlist TEXT NULL;
ALTER TABLE capturecard ADD COLUMN vbidevice VARCHAR(255);
REPLACE INTO settings (value, data) VALUES ("LCDHost","localhost");
REPLACE INTO settings (value, data) VALUES ("LCDPort","13666");
