TEMPLATE = lib
TARGET = dvbdev
CONFIG += thread staticlib warn_off

include ( ../../settings.pro )
include ( ../../config.mak )

INCLUDEPATH += ../../

QMAKE_CFLAGS_RELEASE = $$OPTFLAGS -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_GNU_SOURCE
QMAKE_CFLAGS_DEBUG = -g -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_GNU_SOURCE

# Input
using_dvb {
    HEADERS += dvbdev.h tune.h dvb_defaults.h transform.h remux.h ctools.h ringbuffy.h
    SOURCES += dvbdev.c tune.c transform.c remux.c ctools.c ringbuffy.c
}

!contains(DEFINES, OLDSTRUCT) {
  DEFINES += NEWSTRUCT
}
