quit
#
#   Make sure you read the README in this directory and
#   alter the value for MY_HOST_NAME. You can then 
#   remove the first line of this file (quit) and
#   run it through mysql. 
#
USE mythconverg;

ALTER TABLE musicplaylist ADD COLUMN hostname VARCHAR(255);

#
#   Change MY_HOST_NAME to the base name of the
#   computer you will be runnning mythmusic from
#

UPDATE musicplaylist SET hostname = "MY_HOST_NAME";
