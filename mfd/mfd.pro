!exists(./config.pro ) {
    error(Missing config.pro: please run the configure script)
}

TEMPLATE = subdirs


# Directories
SUBDIRS += mdcaplib mfdlib mfd mfdctl clientlib

include(config.pro)
