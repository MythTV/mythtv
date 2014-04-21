include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
using_frontend {
    SUBDIRS += mythavtest mythfrontend mythcommflag
    SUBDIRS += mythjobqueue mythlcdserver
    contains(CONFIG_MYTHLOGSERVER, "yes") {
        SUBDIRS += mythlogserver
    }
    SUBDIRS += mythwelcome mythshutdown mythutil
    SUBDIRS += mythpreviewgen mythmediaserver mythccextractor
    SUBDIRS += mythscreenwizard
    !mingw:!win32-msvc*: SUBDIRS += mythtranscode/replex
}

using_backend {
    SUBDIRS += mythbackend mythfilldatabase mythtv-setup 
    SUBDIRS += mythmetadatalookup

    !win32-msvc*:SUBDIRS += scripts
    !mingw:!win32-msvc*: SUBDIRS += mythfilerecorder
}

using_mythtranscode: SUBDIRS += mythtranscode
