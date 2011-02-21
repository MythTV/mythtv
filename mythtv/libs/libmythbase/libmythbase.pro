include ( ../../settings.pro )
#include ( ../../version.pro )

TEMPLATE = lib
TARGET = mythbase-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += mythsocket.h mythsocket_cb.h mythsocketthread.h msocketdevice.h
HEADERS += mythbaseexp.h mythdbcon.h mythdb.h mythdbparams.h oldsettings.h
HEADERS += mythverbose.h mythversion.h compat.h mythconfig.h
HEADERS += mythobservable.h mythevent.h httpcomms.h mcodecs.h
HEADERS += mythtimer.h mythsignalingtimer.h mythdirs.h exitcodes.h
HEADERS += lcddevice.h mythstorage.h remotefile.h decodeencode.h
HEADERS += mythcorecontext.h mythsystem.h mythlocale.h storagegroup.h
HEADERS += mythcoreutil.h mythdownloadmanager.h mythtranslation.h
HEADERS += unzip.h unzip_p.h zipentry_p.h iso639.h iso3166.h mythmedia.h
HEADERS += util.h mythhdd.h mythcdrom.h autodeletedeque.h dbutil.h
HEADERS += mythhttppool.h mythhttphandler.h mythdeque.h

SOURCES += mythsocket.cpp mythsocketthread.cpp msocketdevice.cpp
SOURCES += mythdbcon.cpp mythdb.cpp oldsettings.cpp mythverbose.cpp
SOURCES += mythobservable.cpp mythevent.cpp httpcomms.cpp mcodecs.cpp
SOURCES += mythdirs.cpp mythsignalingtimer.cpp
SOURCES += lcddevice.cpp mythstorage.cpp remotefile.cpp decodeencode.cpp
SOURCES += mythcorecontext.cpp mythsystem.cpp mythlocale.cpp storagegroup.cpp
SOURCES += mythcoreutil.cpp mythdownloadmanager.cpp mythtranslation.cpp
SOURCES += unzip.cpp iso639.cpp iso3166.cpp mythmedia.cpp util.cpp
SOURCES += mythhdd.cpp mythcdrom.cpp dbutil.cpp
SOURCES += mythhttppool.cpp mythhttphandler.cpp

win32:SOURCES += msocketdevice_win.cpp
unix {
    SOURCES += msocketdevice_unix.cpp system-unix.cpp
    HEADERS += system-unix.h
    QMAKE_CXXFLAGS += -fno-strict-aliasing
}

mingw {
    SOURCES += system-windows.cpp
    HEADERS += system-windows.h
}

# Install headers to same location as libmyth to make things easier
inc.path = $${PREFIX}/include/mythtv/
inc.files  = mythverbose.h mythdbcon.h mythdbparams.h mythbaseexp.h mythdb.h
inc.files += compat.h mythversion.h mythconfig.h mythconfig.mak
inc.files += mythobservable.h mythevent.h httpcomms.h mcodecs.h
inc.files += mythtimer.h lcddevice.h exitcodes.h mythdirs.h mythstorage.h
inc.files += mythsocket.h mythsocket_cb.h msocketdevice.h
inc.files += mythcorecontext.h mythsystem.h storagegroup.h
inc.files += mythcoreutil.h mythlocale.h mythdownloadmanager.h
inc.files += mythtranslation.h iso639.h iso3166.h mythmedia.h util.h
inc.files += mythcdrom.h autodeletedeque.h dbutil.h mythhttppool.h mythdeque.h

# Allow both #include <blah.h> and #include <libmyth/blah.h>
inc2.path  = $${PREFIX}/include/mythtv/libmyth
inc2.files = $${inc.files}

inc3.path  = $${PREFIX}/include/mythtv/libmythbase
inc3.files = $${inc.files}

INSTALLS += inc inc2 inc3

DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES += LIBDIRNAME=\\\"$${LIBDIRNAME}\\\"
DEFINES += MBASE_API

linux:DEFINES += linux

macx {
    HEADERS += mythcdrom-darwin.h
    SOURCES += mythcdrom-darwin.cpp

    # Mac OS X Frameworks
    FWKS = IOKit

    # The following trick is tidier, and shortens the command line, but it
    # depends on the shell expanding Csh-style braces. Luckily, Bash & Zsh do.
    FC = $$join(FWKS,",","{","}")

    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/$${FC}.framework/Frameworks
    LIBS           += -framework $$join(FWKS," -framework ")
}

linux {
    SOURCES += mythcdrom-linux.cpp
    HEADERS += mythcdrom-linux.h
}

freebsd {
    SOURCES += mythcdrom-freebsd.cpp
    HEADERS += mythcdrom-freebsd.h
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

using_libudf {
    DEFINES += USING_LIBUDF
    LIBS += -ludf
}

mingw:LIBS += -lws2_32

QT += xml sql network

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
