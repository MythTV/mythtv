TEMPLATE = lib
TARGET = dvbdev
CONFIG += thread staticlib warn_off

include ( ../../settings.pro )

INCLUDEPATH += ../../

QMAKE_CFLAGS_RELEASE = $$OPTFLAGS -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_GNU_SOURCE
QMAKE_CFLAGS_DEBUG = -g -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_GNU_SOURCE

using_dvb {
    HEADERS += dvbdev.h transform.c remux.h ringbuffy.h ctools.h dvbci.h
    SOURCES += dvbdev.c transform.h remux.c ringbuffy.c ctools.c dvbci.cpp
}

