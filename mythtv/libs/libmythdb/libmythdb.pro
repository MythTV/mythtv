include ( ../../settings.pro )
include ( ../../version.pro )

TEMPLATE = lib
TARGET = mythdb-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += mythsocket.h mythsocket_cb.h mythsocketthread.h msocketdevice.h
HEADERS += mythexp.h mythdbcon.h mythdb.h mythdbparams.h oldsettings.h
HEADERS += mythverbose.h mythversion.h compat.h mythconfig.h
HEADERS += mythobservable.h mythevent.h httpcomms.h mcodecs.h
HEADERS += mythtimer.h mythsignalingtimer.h mythdirs.h exitcodes.h
HEADERS += lcddevice.h mythstorage.h remotefile.h decodeencode.h
HEADERS += mythcorecontext.h mythsystem.h mythlocale.h storagegroup.h
HEADERS += mythcoreutil.h mythdownloadmanager.h mythtranslation.h
HEADERS += unzip.h unzip_p.h zipentry_p.h

SOURCES += mythsocket.cpp mythsocketthread.cpp msocketdevice.cpp
SOURCES += mythdbcon.cpp mythdb.cpp oldsettings.cpp mythverbose.cpp
SOURCES += mythobservable.cpp mythevent.cpp httpcomms.cpp mcodecs.cpp
SOURCES += mythdirs.cpp mythsignalingtimer.cpp
SOURCES += lcddevice.cpp mythstorage.cpp remotefile.cpp decodeencode.cpp
SOURCES += mythcorecontext.cpp mythsystem.cpp mythlocale.cpp storagegroup.cpp
SOURCES += mythcoreutil.cpp mythdownloadmanager.cpp mythtranslation.cpp
SOURCES += unzip.cpp

win32:SOURCES += msocketdevice_win.cpp
unix {
    SOURCES += msocketdevice_unix.cpp
    QMAKE_CXXFLAGS += -fno-strict-aliasing
}

# Install headers to same location as libmyth to make things easier
inc.path = $${PREFIX}/include/mythtv/
inc.files  = mythverbose.h mythdbcon.h mythdbparams.h mythexp.h mythdb.h
inc.files += compat.h mythversion.h mythconfig.h mythconfig.mak
inc.files += mythobservable.h mythevent.h httpcomms.h mcodecs.h
inc.files += mythtimer.h lcddevice.h exitcodes.h mythdirs.h mythstorage.h
inc.files += mythsocket.h mythsocket_cb.h msocketdevice.h
inc.files += mythcorecontext.h mythsystem.h storagegroup.h
inc.files += mythcoreutil.h mythlocale.h mythdownloadmanager.h
inc.files += mythtranslation.h

# Allow both #include <blah.h> and #include <libmyth/blah.h>
inc2.path  = $${PREFIX}/include/mythtv/libmyth
inc2.files = $${inc.files}

inc3.path  = $${PREFIX}/include/mythtv/libmythdb
inc3.files = $${inc.files}

INSTALLS += inc inc2 inc3

DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES += LIBDIRNAME=\\\"$${LIBDIRNAME}\\\"

linux:DEFINES += linux

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

mingw:LIBS += -lpthread -lws2_32

QT += xml sql network

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
