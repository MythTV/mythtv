include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythudfread-$$LIBVERSION
CONFIG += thread staticlib warn_off
# CONFIG -= qt
target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ../../libs/libmythbase

win32-msvc* {
    # needed for vcxproj
    QMAKE_CXXFLAGS += /TP "/FI compat.h" "/FI fcntl.h"

    # needed for nmake
    QMAKE_CFLAGS   += /TP "/FI compat.h" "/FI fcntl.h"
}

QMAKE_CLEAN += $(TARGET)

HEADERS += blockinput.h default_blockinput.h ecma167.h udfread.h udfread-version.h
SOURCES += default_blockinput.c ecma167.c udfread.c udfread-version.c
DEFINES += HAVE_CONFIG_H

inc.path = $${PREFIX}/include/mythtv/
inc.files = blockinput.h udfread.h

INSTALLS += inc

mingw {
    DEFINES += STDC_HEADERS
    dll : contains( TEMPLATE, lib ) {

        # Qt under Linux/UnixMac OS X builds libBlah.a and libBlah.so,
        # but is using the Windows defaults libBlah.a and Blah.dll.
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
