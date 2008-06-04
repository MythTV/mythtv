include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythdvdnav-$$LIBVERSION
CONFIG += thread staticlib warn_off
target.path = $${LIBDIR}

INCLUDEPATH += ../ ../../ ../libmyth

#build position independent code since the library is linked into a shared library
QMAKE_CFLAGS += -fPIC -DPIC

DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE DVDNAV_COMPILE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += bswap.h decoder.h dvd_input.h dvdnav_events.h dvdnav.h 
HEADERS += dvdnav_internal.h dvd_reader.h dvdread_internal.h dvd_types.h
HEADERS += dvd_udf.h ifo_read.h ifo_types.h md5.h nav_print.h nav_read.h
HEADERS += nav_types.h read_cache.h remap.h vmcmd.h vm.h

SOURCES += decoder.c dvd_input.c dvdnav.c dvd_reader.c dvd_udf.c highlight.c
SOURCES += ifo_read.c md5.c navigation.c nav_print.c nav_read.c read_cache.c
SOURCES += remap.c searching.c settings.c vm.c vmcmd.c

inc.path = $${PREFIX}/include/mythtv/dvdnav
inc.files = dvdnav_events.h dvd_reader.h ifo_types.h nav_read.h dvdnav.h
inc.files += dvd_types.h ifo_read.h nav_print.h nav_types.h

INSTALLS += target inc


macx {
    # Globals in static libraries need special treatment on OS X
    QMAKE_CFLAGS += -fno-common
}

mingw:DEFINES += STDC_HEADERS

include ( ../libs-targetfix.pro )
