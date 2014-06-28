include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythminilzo-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt

target.path = $${LIBDIR}

INCLUDEPATH += ../ ../../

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# LZO used by NuppelDecoder & NuppelVideoRecorder
HEADERS += lzoconf.h minilzo.h
SOURCES += minilzo.c

inc.path = $${PREFIX}/include/mythtv/
inc.files = minilzo.h

INSTALLS += inc

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
