include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythbluray-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt
target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./bdnav
INCLUDEPATH += ../libmythdb

#build position independent code since the library is linked into a shared library
QMAKE_CFLAGS += -fPIC -DPIC

DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE USING_DLOPEN

# DEFINES += LOG_DEBUG TRACE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# bdnav
HEADERS += bluray.h register.h bdnav/*.h hdmv/*.h util/*.h file/*.h
#HEADERS += bdj/*.h

SOURCES += bluray.c register.c bdnav/*.c hdmv/*.c file/*.c util/*.c
#HEADERS += bdj/*.c

inc_bdnav.path = $${PREFIX}/include/mythtv/bluray
inc_bdnav.files = bluray.h bdnav/*.h hdmv/*.h file/*.h util/*.h

INSTALLS += target inc_bdnav

macx {
    # Globals in static libraries need special treatment on OS X
    QMAKE_CFLAGS += -fno-common
}

mingw:DEFINES += STDC_HEADERS

include ( ../libs-targetfix.pro )
