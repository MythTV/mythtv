CREATE DATABASE if not exists mythconverg;
GRANT ALL ON mythconverg.* TO mythtv@localhost IDENTIFIED BY "mythtv";
FLUSH PRIVILEGES;
GRANT CREATE TEMPORARY TABLES ON mythconverg.* TO mythtv@localhost IDENTIFIED BY "mythtv";
FLUSH PRIVILEGES;
