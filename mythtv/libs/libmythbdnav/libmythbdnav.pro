include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythbdnav-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt
target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./libbdnav
INCLUDEPATH += ../libmythdb

#build position independent code since the library is linked into a shared library
QMAKE_CFLAGS += -fPIC -DPIC

DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE

# DEFINES += LOG_DEBUG TRACE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# bdnav
HEADERS += bluray.h
HEADERS += libbdnav/bdparse.h  libbdnav/clpi_parse.h  libbdnav/mpls_parse.h  libbdnav/navigation.h
HEADERS += file/configfile.h  file/dir.h  file/dl.h  file/file.h
HEADERS += util/attributes.h  util/bits.h  util/logging.h  util/macro.h  util/strutl.h

SOURCES += bluray.c
SOURCES += libbdnav/clpi_parse.c  libbdnav/mpls_parse.c  libbdnav/navigation.c
SOURCES += file/configfile.c  file/dir_posix.c  file/dl_posix.c  file/file_posix.c
SOURCES +=util/logging.c  util/strutl.c

inc_bdnav.path = $${PREFIX}/include/mythtv/bdnav
inc_bdnav.files = libbdnav/bdparse.h  libbdnav/clpi_parse.h  libbdnav/mpls_parse.h  libbdnav/navigation.h  bluray.h
inc_bdnav.files += file/configfile.h  file/dir.h  file/dl.h  file/file.h
inc_bdnav.files += util/attributes.h  util/bits.h  util/logging.h  util/macro.h  util/strutl.h

INSTALLS += target inc_bdnav

macx {
    # Globals in static libraries need special treatment on OS X
    QMAKE_CFLAGS += -fno-common
}

mingw:DEFINES += STDC_HEADERS

include ( ../libs-targetfix.pro )
