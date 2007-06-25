!exists(./config.pro ) {
    error(Missing config.pro: please run the configure script)
}
include ( config.pro )

TEMPLATE = subdirs

# Directories
SUBDIRS += mdcaplib mtcplib mfdlib mfd mfdctl
