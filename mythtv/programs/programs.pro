include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
using_frontend {
    SUBDIRS += mythavtest mythfrontend mythcommflag
    SUBDIRS += mythjobqueue mythlcdserver mythlogserver
    SUBDIRS += mythwelcome mythshutdown mythutil
    SUBDIRS += mythpreviewgen mythmediaserver mythccextractor
    SUBDIRS += mythscreenwizard
    !mingw: SUBDIRS += mythtranscode/replex
}

using_backend {
    SUBDIRS += mythbackend mythfilldatabase mythtv-setup scripts
    SUBDIRS += mythmetadatalookup
}

using_mythtranscode: SUBDIRS += mythtranscode
