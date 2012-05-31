include ( ../../settings.pro )
#include ( ../../version.pro )

TEMPLATE = lib
TARGET = mythbase-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += mthread.h mthreadpool.h
HEADERS += mythsocket.h mythsocket_cb.h mythsocketthread.h msocketdevice.h
HEADERS += mythbaseexp.h mythdbcon.h mythdb.h mythdbparams.h oldsettings.h
HEADERS += verbosedefs.h mythversion.h compat.h mythconfig.h
HEADERS += mythobservable.h mythevent.h httpcomms.h mcodecs.h
HEADERS += mythtimer.h mythsignalingtimer.h mythdirs.h exitcodes.h
HEADERS += lcddevice.h mythstorage.h remotefile.h logging.h
HEADERS += mythcorecontext.h mythsystem.h mythlocale.h storagegroup.h
HEADERS += mythcoreutil.h mythdownloadmanager.h mythtranslation.h
HEADERS += unzip.h unzip_p.h zipentry_p.h iso639.h iso3166.h mythmedia.h
HEADERS += mythmiscutil.h mythhdd.h mythcdrom.h autodeletedeque.h dbutil.h
HEADERS += mythhttppool.h mythhttphandler.h mythdeque.h mythlogging.h
HEADERS += mythbaseutil.h referencecounter.h version.h mythcommandlineparser.h
HEADERS += mythscheduler.h filesysteminfo.h hardwareprofile.h serverpool.h
HEADERS += plist.h bswap.h
HEADERS += ffmpeg-mmx.h

SOURCES += mthread.cpp mthreadpool.cpp
SOURCES += mythsocket.cpp mythsocketthread.cpp msocketdevice.cpp
SOURCES += mythdbcon.cpp mythdb.cpp mythdbparams.cpp oldsettings.cpp
SOURCES += mythobservable.cpp mythevent.cpp httpcomms.cpp mcodecs.cpp
SOURCES += mythdirs.cpp mythsignalingtimer.cpp
SOURCES += lcddevice.cpp mythstorage.cpp remotefile.cpp
SOURCES += mythcorecontext.cpp mythsystem.cpp mythlocale.cpp storagegroup.cpp
SOURCES += mythcoreutil.cpp mythdownloadmanager.cpp mythtranslation.cpp
SOURCES += unzip.cpp iso639.cpp iso3166.cpp mythmedia.cpp mythmiscutil.cpp
SOURCES += mythhdd.cpp mythcdrom.cpp dbutil.cpp
SOURCES += mythhttppool.cpp mythhttphandler.cpp logging.cpp
SOURCES += referencecounter.cpp mythcommandlineparser.cpp
SOURCES += filesysteminfo.cpp hardwareprofile.cpp serverpool.cpp
SOURCES += plist.cpp

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
inc.files  = mythdbcon.h mythdbparams.h mythbaseexp.h mythdb.h
inc.files += compat.h mythversion.h mythconfig.h mythconfig.mak version.h
inc.files += mythobservable.h mythevent.h httpcomms.h mcodecs.h verbosedefs.h
inc.files += mythtimer.h lcddevice.h exitcodes.h mythdirs.h mythstorage.h
inc.files += mythsocket.h mythsocket_cb.h msocketdevice.h mythlogging.h
inc.files += mythcorecontext.h mythsystem.h storagegroup.h
inc.files += mythcoreutil.h mythlocale.h mythdownloadmanager.h
inc.files += mythtranslation.h iso639.h iso3166.h mythmedia.h mythmiscutil.h
inc.files += mythcdrom.h autodeletedeque.h dbutil.h mythhttppool.h mythdeque.h
inc.files += referencecounter.h mythcommandlineparser.h mthread.h mthreadpool.h
inc.files += filesysteminfo.h hardwareprofile.h bonjourregister.h serverpool.h
inc.files += plist.h bswap.h ffmpeg-mmx.h

# Allow both #include <blah.h> and #include <libmythbase/blah.h>
inc2.path  = $${PREFIX}/include/mythtv/libmythbase
inc2.files = $${inc.files}

INSTALLS += inc inc2

DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES += LIBDIRNAME=\\\"$${LIBDIRNAME}\\\"
DEFINES += MBASE_API

linux:DEFINES += linux

macx {
    HEADERS += mythcdrom-darwin.h
    SOURCES += mythcdrom-darwin.cpp

    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/IOKit.framework/Frameworks
    LIBS           += -framework IOKit
}

linux {
    SOURCES += mythcdrom-linux.cpp
    HEADERS += mythcdrom-linux.h
}

freebsd {
    SOURCES += mythcdrom-freebsd.cpp
    HEADERS += mythcdrom-freebsd.h
    LIBS += -lthr -lc
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

using_libdns_sd {
    DEFINES += USING_LIBDNS_SD
    HEADERS += bonjourregister.h
    SOURCES += bonjourregister.cpp
    !macx: LIBS += -ldns_sd
}

using_libudf {
    DEFINES += USING_LIBUDF
    LIBS += -ludf
}

using_x11:DEFINES += USING_X11

mingw:LIBS += -lws2_32

QT += xml sql network

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
