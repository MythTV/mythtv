
TEMPLATE = lib
TARGET = vbitext
CONFIG += thread staticlib warn_off

include ( ../../settings.pro )
include ( ../../config.mak )

INCLUDEPATH = ../../

QMAKE_CFLAGS_RELEASE = $$OPTFLAGS -DHAVE_AV_CONFIG_H -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
QMAKE_CFLAGS_DEBUG = -g -DHAVE_AV_CONFIG_H -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE

# Input
HEADERS += cc.h dllist.h hamm.h lang.h vbi.h vt.h
SOURCES += cc.c vbi.c hamm.c lang.c

