include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
using_frontend {
    SUBDIRS += mythavtest mythfrontend mythcommflag
    SUBDIRS += mythtvosd mythjobqueue mythlcdserver
    SUBDIRS += mythwelcome mythshutdown mythtranscode/replex
    SUBDIRS += mythpreviewgen
}

using_backend {
    SUBDIRS += mythbackend mythfilldatabase mythtv-setup scripts
}

using_frontend:using_backend {
    SUBDIRS += mythtranscode
}

mingw: SUBDIRS -= mythtranscode mythtranscode/replex
