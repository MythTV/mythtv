# File: legacy.mc.sql
#
# For:
#   MariaDB versions below 10.3

# Database creation:
CREATE DATABASE IF NOT EXISTS mythconverg;

# Localhost user creation.
GRANT ALL ON mythconverg.* TO mythtv@localhost IDENTIFIED BY 'mythtv';
FLUSH PRIVILEGES;

# Repeate the above for a mythtv user on remote host(s). For example,
# for all hosts on 192.168.1.0/24:
# GRANT ALL ON mythconverg.* TO 'mythtv'@'192.168.1.%' IDENTIFIED BY 'mythtv';
# FLUSH PRIVILEGES;

# Temporary table privileges required for backend only (not additional users.)
GRANT CREATE TEMPORARY TABLES ON mythconverg.* TO mythtv@localhost IDENTIFIED BY 'mythtv';
FLUSH PRIVILEGES;
ALTER DATABASE mythconverg DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;
