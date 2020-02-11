CREATE DATABASE IF NOT EXISTS mythconverg;
CREATE USER IF NOT EXISTS 'mythtv'@'localhost' IDENTIFIED BY 'mythtv';
ALTER USER 'mythtv'@'localhost' IDENTIFIED WITH mysql_native_password;
GRANT ALL ON mythconverg.* TO mythtv@localhost;
FLUSH PRIVILEGES;
GRANT CREATE TEMPORARY TABLES ON mythconverg.* TO mythtv@localhost;
FLUSH PRIVILEGES;
ALTER DATABASE mythconverg DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;
