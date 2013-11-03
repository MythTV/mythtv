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
HEADERS += mythsocket.h mythsocket_cb.h
HEADERS += mythbaseexp.h mythdbcon.h mythdb.h mythdbparams.h oldsettings.h
HEADERS += verbosedefs.h mythversion.h compat.h mythconfig.h
HEADERS += mythobservable.h mythevent.h
HEADERS += mythtimer.h mythsignalingtimer.h mythdirs.h exitcodes.h
HEADERS += lcddevice.h mythstorage.h remotefile.h logging.h loggingserver.h
HEADERS += mythcorecontext.h mythsystem.h mythsystemprivate.h
HEADERS += mythlocale.h storagegroup.h
HEADERS += mythcoreutil.h mythdownloadmanager.h mythtranslation.h
HEADERS += unzip.h unzip_p.h zipentry_p.h iso639.h iso3166.h mythmedia.h
HEADERS += mythmiscutil.h mythhdd.h mythcdrom.h autodeletedeque.h dbutil.h
HEADERS += mythdeque.h mythlogging.h
HEADERS += mythbaseutil.h referencecounter.h referencecounterlist.h
HEADERS += version.h mythcommandlineparser.h
HEADERS += mythscheduler.h filesysteminfo.h hardwareprofile.h serverpool.h
HEADERS += plist.h bswap.h signalhandling.h mythtimezone.h mythdate.h
HEADERS += mythplugin.h mythpluginapi.h housekeeper.h
HEADERS += ffmpeg-mmx.h
HEADERS += mythsystemlegacy.h mythtypes.h
HEADERS += threadedfilewriter.h mythsingledownload.h

SOURCES += mthread.cpp mthreadpool.cpp
SOURCES += mythsocket.cpp
SOURCES += mythdbcon.cpp mythdb.cpp mythdbparams.cpp oldsettings.cpp
SOURCES += mythobservable.cpp mythevent.cpp
SOURCES += mythtimer.cpp mythsignalingtimer.cpp mythdirs.cpp
SOURCES += lcddevice.cpp mythstorage.cpp remotefile.cpp
SOURCES += mythcorecontext.cpp mythsystem.cpp mythlocale.cpp storagegroup.cpp
SOURCES += mythcoreutil.cpp mythdownloadmanager.cpp mythtranslation.cpp
SOURCES += unzip.cpp iso639.cpp iso3166.cpp mythmedia.cpp mythmiscutil.cpp
SOURCES += mythhdd.cpp mythcdrom.cpp dbutil.cpp
SOURCES += logging.cpp loggingserver.cpp
SOURCES += referencecounter.cpp mythcommandlineparser.cpp
SOURCES += filesysteminfo.cpp hardwareprofile.cpp serverpool.cpp
SOURCES += plist.cpp signalhandling.cpp mythtimezone.cpp mythdate.cpp
SOURCES += mythplugin.cpp housekeeper.cpp
SOURCES += mythsystemlegacy.cpp mythtypes.cpp
SOURCES += threadedfilewriter.cpp mythsingledownload.cpp

# This stuff is not Qt5 compatible..
contains(QT_VERSION, ^4\\.[0-9]\\..*) {
HEADERS += mcodecs.h
SOURCES += mcodecs.cpp
}

unix {
    SOURCES += mythsystemunix.cpp
    HEADERS += mythsystemunix.h
}

mingw | win32-msvc* {
    SOURCES += mythsystemwindows.cpp
    HEADERS += mythsystemwindows.h
}

# Install headers to same location as libmyth to make things easier
inc.path = $${PREFIX}/include/mythtv/
inc.files += mythdbcon.h mythdbparams.h mythbaseexp.h mythdb.h
inc.files += compat.h mythversion.h mythconfig.h mythconfig.mak version.h
inc.files += mythobservable.h mythevent.h mcodecs.h verbosedefs.h
inc.files += mythtimer.h lcddevice.h exitcodes.h mythdirs.h mythstorage.h
inc.files += mythsocket.h mythsocket_cb.h mythlogging.h
inc.files += mythcorecontext.h mythsystem.h storagegroup.h loggingserver.h
inc.files += mythcoreutil.h mythlocale.h mythdownloadmanager.h
inc.files += mythtranslation.h iso639.h iso3166.h mythmedia.h mythmiscutil.h
inc.files += mythcdrom.h autodeletedeque.h dbutil.h mythdeque.h
inc.files += referencecounter.h referencecounterlist.h mythcommandlineparser.h
inc.files += mthread.h mthreadpool.h
inc.files += filesysteminfo.h hardwareprofile.h bonjourregister.h serverpool.h
inc.files += plist.h bswap.h signalhandling.h ffmpeg-mmx.h mythdate.h
inc.files += mythplugin.h mythpluginapi.h mythqtcompat.h
inc.files += remotefile.h mythsystemlegacy.h mythtypes.h
inc.files += threadedfilewriter.h mythsingledownload.h

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

win32-msvc* {

    LIBS += -lws2_32
    EXTRA_LIBS += -lzlib
    DEFINES += NOLOGSERVER

    # we need to make sure version.h is generated.

    versionTarget.target  = version.h
    versionTarget.depends = FORCE
    versionTarget.commands = powershell -noprofile -executionpolicy bypass -File ..\..\version.ps1 ..\..

    PRE_TARGETDEPS += version.h
    QMAKE_EXTRA_TARGETS += versionTarget
}

QT += xml sql network

contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
