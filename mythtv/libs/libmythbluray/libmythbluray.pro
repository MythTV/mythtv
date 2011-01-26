include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythbluray-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt
target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./bdnav
INCLUDEPATH += ../libmythbase
INCLUDEPATH += ../libmythtv

DEFINES += HAVE_CONFIG_H DLOPEN_CRYPTO_LIBS HAVE_PTHREAD_H HAVE_DIRENT_H

using_libxml2 {
DEFINES += HAVE_LIBXML2
}

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# bdnav
HEADERS += bluray.h register.h bdnav/*.h hdmv/*.h util/*.h file/*.h decoders/*.h
SOURCES += bluray.c register.c bdnav/*.c hdmv/*.c file/*.c util/*.c decoders/*.c

inc_bdnav.path = $${PREFIX}/include/mythtv/bluray
inc_bdnav.files = bluray.h bdnav/*.h hdmv/*.h file/*.h util/*.h

INSTALLS += target inc_bdnav

mingw:DEFINES += STDC_HEADERS

using_bdjava {
HEADERS += bdj/*.h
SOURCES += bdj/*.c

QMAKE_POST_LINK=/$${ANTBIN} -f bdj/build.xml; /$${ANTBIN} -f bdj/build.xml clean

installjar.path = $${PREFIX}/share/mythtv/jars
installjar.files = libmythbluray.jar

INSTALLS += installjar

QMAKE_CLEAN += libmythbluray.jar
}

include ( ../libs-targetfix.pro )
