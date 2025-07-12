include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythudfread-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt
target.path = $${LIBDIR}

# can't use config.h because code only checks if defined
DEFINES += HAVE_CONFIG_H=0
DEFINES += HAVE_UDFREAD_VERSION_H
equals(HAVE_PTHREAD_H, "yes") : DEFINES += HAVE_PTHREAD_H
equals(HAVE_UNISTD_H,  "yes") : DEFINES += HAVE_UNISTD_H
equals(HAVE_FCNTL_H,   "yes") : DEFINES += HAVE_FCNTL_H

win32-msvc* {
    config += qt # should not be necessary, since this is C not C++
    INCLUDEPATH += ../../libs/libmythbase # for compat.h
    # needed for vcxproj
    QMAKE_CXXFLAGS += /TP "/FI compat.h" "/FI fcntl.h"

    # needed for nmake
    QMAKE_CFLAGS   += /TP "/FI compat.h" "/FI fcntl.h"
}

QMAKE_CLEAN += $(TARGET)

HEADERS += attributes.h
HEADERS += blockinput.h default_blockinput.h ecma167.h udfread.h udfread-version.h
SOURCES += default_blockinput.c ecma167.c udfread.c udfread-version.c

inc.path = $${PREFIX}/include/mythtv/
inc.files = blockinput.h udfread.h udfread-version.h

INSTALLS += inc

include (../../libs/libs-targetfix.pro)
