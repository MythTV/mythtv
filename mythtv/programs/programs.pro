include ( ../config.mak )
include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
using_frontend {
    SUBDIRS += mythepg mythprogfind mythtv mythfrontend mythcommflag mythlcd 
    SUBDIRS += mythtvosd mythjobqueue mythuitest
}

using_backend {
    SUBDIRS += mythbackend mythfilldatabase
}

using_frontend:using_backend {
    SUBDIRS += mythtranscode
}
