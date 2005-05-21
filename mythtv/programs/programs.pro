include ( ../config.mak )
include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
SUBDIRS += mythepg mythprogfind mythtv mythfrontend mythcommflag mythlcd 
SUBDIRS += mythtvosd mythjobqueue mythuitest

#
#	mythuitest  should(?) go back into SUBDIRS (above) when we've figured
#	out what config values it will need to read
#

backend {
    SUBDIRS += mythbackend mythfilldatabase mythtranscode
}

