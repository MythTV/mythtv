USE mythconverg;
ALTER TABLE allrecord ADD chanid INT UNSIGNED NULL;

ALTER TABLE channel CHANGE chanid chanid INT UNSIGNED NOT NULL;
ALTER TABLE channel ADD xmltvid VARCHAR(64) NULL;

