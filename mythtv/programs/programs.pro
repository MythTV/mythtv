include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
using_frontend {
    SUBDIRS += mythavtest mythfrontend mythcommflag
    SUBDIRS += mythjobqueue mythlcdserver
    SUBDIRS += mythwelcome mythshutdown mythutil
    SUBDIRS += mythpreviewgen mythmediaserver mythccextractor
    SUBDIRS += mythscreenwizard
    !mingw:!win32-msvc*: SUBDIRS += mythtranscode/external/replex
}

using_backend {
    SUBDIRS += mythbackend mythfilldatabase mythtv-setup 
    SUBDIRS += mythmetadatalookup

    !win32-msvc*:SUBDIRS += scripts
    !mingw:!win32-msvc*: SUBDIRS += mythfilerecorder
    !mingw:!win32-msvc*: SUBDIRS += mythexternrecorder
}

using_mythtranscode: SUBDIRS += mythtranscode
