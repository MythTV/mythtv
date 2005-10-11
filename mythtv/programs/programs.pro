include ( ../config.mak )
include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
using_frontend {
    SUBDIRS += mythtv mythfrontend mythcommflag
    SUBDIRS += mythtvosd mythjobqueue mythuitest mythlcdserver
}

using_backend {
    SUBDIRS += mythbackend mythfilldatabase
}

using_frontend:using_backend {
    SUBDIRS += mythtranscode
}
