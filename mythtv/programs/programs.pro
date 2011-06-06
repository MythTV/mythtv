include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
using_frontend {
    SUBDIRS += mythavtest mythfrontend mythcommflag
    SUBDIRS += mythmessage mythjobqueue mythlcdserver
    SUBDIRS += mythwelcome mythshutdown
    SUBDIRS += mythpreviewgen mythmediaserver
    !mingw: SUBDIRS += mythtranscode/replex
}

using_backend {
    SUBDIRS += mythbackend mythfilldatabase mythtv-setup scripts
}

using_mythtranscode: SUBDIRS += mythtranscode
