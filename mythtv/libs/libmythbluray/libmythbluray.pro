include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythbluray-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt
target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./bdnav
INCLUDEPATH += ../libmythdb

DEFINES += HAVE_CONFIG_H DLOPEN_CRYPTO_LIBS

# DEFINES += LOG_DEBUG TRACE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# bdnav
HEADERS += bluray.h register.h bdnav/*.h hdmv/*.h util/*.h file/*.h decoders/*.h
#HEADERS += bdj/*.h

SOURCES += bluray.c register.c bdnav/*.c hdmv/*.c file/*.c util/*.c decoders/*.c
#SOURCES += bdj/*.c

inc_bdnav.path = $${PREFIX}/include/mythtv/bluray
inc_bdnav.files = bluray.h bdnav/*.h hdmv/*.h file/*.h util/*.h

INSTALLS += target inc_bdnav


mingw:DEFINES += STDC_HEADERS

include ( ../libs-targetfix.pro )
