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
HEADERS += bluray.h libdbdnav/*.h util/*.h file/*.h

SOURCES += bluray.c libbdnav/*.c file/*.c util/*.c

inc_bdnav.path = $${PREFIX}/include/mythtv/bdnav
inc_bdnav.files = bluray.h libbdnav/*.h file/*.h util/*.h

INSTALLS += target inc_bdnav

macx {
    # Globals in static libraries need special treatment on OS X
    QMAKE_CFLAGS += -fno-common
}

mingw:DEFINES += STDC_HEADERS

include ( ../libs-targetfix.pro )
