include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
SUBDIRS += mythepg mythprogfind mythtv mythfrontend mythcommflag mythlcd 
SUBDIRS += mythtvosd

backend {
    SUBDIRS += mythbackend mythfilldatabase mythtranscode
}

