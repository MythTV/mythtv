USE mythconverg;

# The transcoding table now contains different information
# than it originally did.  It is not safe to keep any existing
# rows.
DROP TABLE IF EXISTS transcoding;
CREATE TABLE transcoding (chanid INT UNSIGNED, starttime TIMESTAMP,
                          status INT, hostname VARCHAR(255));
INSERT INTO settings VALUES ('TranscoderUseCutlist', '0', NULL);

