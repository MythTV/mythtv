include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythdvdnav-$$LIBVERSION
CONFIG += thread staticlib warn_off

INCLUDEPATH += ../../

QMAKE_CFLAGS_RELEASE = $$OPTFLAGS -DHAVE_AV_CONFIG_H -I.. -fPIC -DPIC -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
QMAKE_CFLAGS_DEBUG = -g -DHAVE_AV_CONFIG_H -I.. -fPIC -DPIC -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

DEFINES += DVDNAV_COMPILE

# Input
HEADERS += bswap.h decoder.h dvd_input.h dvdnav_events.h dvdnav.h 
HEADERS += dvdnav_internal.h dvd_reader.h dvdread_internal.h dvd_types.h
HEADERS += dvd_udf.h ifo_read.h ifo_types.h md5.h nav_print.h nav_read.h
HEADERS += nav_types.h read_cache.h remap.h vmcmd.h vm.h

SOURCES += decoder.c dvd_input.c dvdnav.c dvd_reader.c dvd_udf.c highlight.c
SOURCES += ifo_read.c md5.c navigation.c nav_print.c nav_read.c read_cache.c
SOURCES += remap.c searching.c settings.c vm.c vmcmd.c



