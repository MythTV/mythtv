!exists(./config.pro ) {
    error(Missing config.pro: please run the configure script)
}

TEMPLATE = subdirs


# Directories
SUBDIRS += mdcaplib mfdlib mfd mfdctl

include(config.pro)
