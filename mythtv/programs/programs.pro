include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
SUBDIRS += mythepg mythprogfind mythtv mythfrontend mythcommflag mythlcd 
SUBDIRS += mythtvosd mythuitest

backend {
    SUBDIRS += mythbackend mythfilldatabase mythtranscode
}

