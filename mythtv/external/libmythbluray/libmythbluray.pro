include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythbluray-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt
target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./bdnav
INCLUDEPATH += ../../libs/libmythbase
INCLUDEPATH += ../../libs/libmythtv

DEFINES += HAVE_CONFIG_H DLOPEN_CRYPTO_LIBS HAVE_PTHREAD_H HAVE_DIRENT_H HAVE_STRINGS_H

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

mingw {
    dll : contains( TEMPLATE, lib ) {

        # Qt under Linux/UnixMac OS X builds libBlah.a and libBlah.so,
        # but is using the Windows defaults  libBlah.a and    Blah.dll.
        #
        # So that our dependency targets work between SUBDIRS, override:
        #
        TARGET = lib$${TARGET}


        # Windows doesn't have a nice variable like LD_LIBRARY_PATH,
        # which means make install would be broken without extra steps.
        # As a workaround, we store dlls with exes. Also improves debugging!
        #
        target.path = $${PREFIX}/bin
    }
}
