!exists(./config.pro ) {
    error(Missing config.pro: please run the configure script)
}

TEMPLATE = subdirs


# Directories
SUBDIRS += mfdlib mfd mfdctl

include(config.pro)
