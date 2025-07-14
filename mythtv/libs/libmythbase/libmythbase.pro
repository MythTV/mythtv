include ( ../../settings.pro )
#include ( ../../version.pro )

TEMPLATE = lib
TARGET = mythbase-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
QT += xml

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += mthread.h mthreadpool.h mythchrono.h mconcurrent.h
HEADERS += mythsocket.h mythsocket_cb.h
HEADERS += mythbaseexp.h mythdbcon.h mythdb.h mythdbparams.h
HEADERS += verbosedefs.h mythversion.h compat.h mythconfig.h
HEADERS += mythobservable.h mythevent.h
HEADERS += mythtimer.h mythdirs.h exitcodes.h
HEADERS += lcddevice.h mythstorage.h remotefile.h logging.h loggingserver.h
HEADERS += mythcorecontext.h mythsystem.h mythsystemprivate.h
HEADERS += mythlocale.h storagegroup.h
HEADERS += mythdownloadmanager.h mythtranslation.h
HEADERS += unzip2.h iso639.h iso3166.h mythmedia.h
HEADERS += mythmiscutil.h mythhdd.h mythcdrom.h autodeletedeque.h dbutil.h
HEADERS += mythdeque.h mythlogging.h
HEADERS += referencecounter.h referencecounterlist.h
HEADERS += version.h mythcommandlineparser.h
HEADERS += mythscheduler.h filesysteminfo.h hardwareprofile.h serverpool.h
HEADERS += mythbinaryplist.h signalhandling.h mythtimezone.h mythdate.h
HEADERS += mythplugin.h mythpluginapi.h housekeeper.h
HEADERS += mythpluginexport.h
HEADERS += mythrandom.h
HEADERS += stringutil.h
HEADERS += mythsystemlegacy.h mythtypes.h
HEADERS += threadedfilewriter.h mythsingledownload.h
HEADERS += ternarycompare.h
HEADERS += mythsession.h
HEADERS += portchecker.h
HEADERS += mythsorthelper.h mythdbcheck.h
HEADERS += mythpower.h
HEADERS += configuration.h
HEADERS += mythappname.h
HEADERS += netgrabbermanager.h
HEADERS += netutils.h
HEADERS += programinfo.h
HEADERS += programinfoupdater.h
HEADERS += programtypes.h
HEADERS += programtypeflags.h
HEADERS += recordingstatus.h
HEADERS += recordingtypes.h
HEADERS += remoteutil.h
HEADERS += rssmanager.h
HEADERS += rssparse.h
HEADERS += unziputil.h
HEADERS += sizetliteral.h

SOURCES += mthread.cpp mthreadpool.cpp
SOURCES += mythsocket.cpp
SOURCES += mythdbcon.cpp mythdb.cpp mythdbparams.cpp
SOURCES += mythobservable.cpp mythevent.cpp
SOURCES += mythtimer.cpp mythdirs.cpp
SOURCES += lcddevice.cpp mythstorage.cpp remotefile.cpp
SOURCES += mythcorecontext.cpp mythsystem.cpp mythlocale.cpp storagegroup.cpp
SOURCES += mythdownloadmanager.cpp mythtranslation.cpp
SOURCES += unzip2.cpp iso639.cpp iso3166.cpp mythmedia.cpp mythmiscutil.cpp
SOURCES += mythhdd.cpp mythcdrom.cpp dbutil.cpp
SOURCES += logging.cpp loggingserver.cpp
SOURCES += referencecounter.cpp mythcommandlineparser.cpp
SOURCES += filesysteminfo.cpp hardwareprofile.cpp serverpool.cpp
SOURCES += mythbinaryplist.cpp signalhandling.cpp mythtimezone.cpp mythdate.cpp
SOURCES += mythplugin.cpp housekeeper.cpp
SOURCES += mythsystemlegacy.cpp mythtypes.cpp
SOURCES += mythrandom.cpp
SOURCES += stringutil.cpp
SOURCES += threadedfilewriter.cpp mythsingledownload.cpp
SOURCES += mythsession.cpp
SOURCES += portchecker.cpp
SOURCES += mythsorthelper.cpp dbcheckcommon.cpp
SOURCES += mythpower.cpp
SOURCES += configuration.cpp
SOURCES += mythversion.cpp
SOURCES += netgrabbermanager.cpp
SOURCES += netutils.cpp
SOURCES += programinfo.cpp
SOURCES += programinfoupdater.cpp
SOURCES += programtypes.cpp
SOURCES += recordingstatus.cpp
SOURCES += recordingtypes.cpp
SOURCES += remoteutil.cpp
SOURCES += rssmanager.cpp
SOURCES += rssparse.cpp
SOURCES += unziputil.cpp

HEADERS += http/mythhttpcommon.h
HEADERS += http/mythhttptypes.h
HEADERS += http/mythhttps.h
HEADERS += http/mythhttpdata.h
HEADERS += http/mythhttpinstance.h
HEADERS += http/mythhttpserver.h
HEADERS += http/mythhttpthread.h
HEADERS += http/mythhttpthreadpool.h
HEADERS += http/mythhttpsocket.h
HEADERS += http/mythwebsocketevent.h
HEADERS += http/mythwebsockettypes.h
HEADERS += http/mythwebsocket.h
HEADERS += http/mythhttpparser.h
HEADERS += http/mythhttprequest.h
HEADERS += http/mythhttpresponse.h
HEADERS += http/mythhttpfile.h
HEADERS += http/mythhttpencoding.h
HEADERS += http/mythhttprewrite.h
HEADERS += http/mythhttproot.h
HEADERS += http/mythhttpranges.h
HEADERS += http/mythhttpcache.h
HEADERS += http/mythhttpservice.h
HEADERS += http/mythhttpmetaservice.h
HEADERS += http/mythhttpmetamethod.h
HEADERS += http/mythhttpservices.h
HEADERS += http/mythmimedatabase.h
HEADERS += http/mythmimetype.h
HEADERS += http/mythwsdl.h
HEADERS += http/mythxsd.h
HEADERS += http/serialisers/mythserialiser.h
HEADERS += http/serialisers/mythjsonserialiser.h
HEADERS += http/serialisers/mythxmlserialiser.h
HEADERS += http/serialisers/mythxmlplistserialiser.h
HEADERS += http/serialisers/mythcborserialiser.h
SOURCES += http/mythhttpcommon.cpp
SOURCES += http/mythhttps.cpp
SOURCES += http/mythhttpdata.cpp
SOURCES += http/mythhttpinstance.cpp
SOURCES += http/mythhttpserver.cpp
SOURCES += http/mythhttpthread.cpp
SOURCES += http/mythhttpthreadpool.cpp
SOURCES += http/mythhttpsocket.cpp
SOURCES += http/mythwebsocketevent.cpp
SOURCES += http/mythwebsockettypes.cpp
SOURCES += http/mythwebsocket.cpp
SOURCES += http/mythhttpparser.cpp
SOURCES += http/mythhttprequest.cpp
SOURCES += http/mythhttpresponse.cpp
SOURCES += http/mythhttpfile.cpp
SOURCES += http/mythhttpencoding.cpp
SOURCES += http/mythhttprewrite.cpp
SOURCES += http/mythhttproot.cpp
SOURCES += http/mythhttpranges.cpp
SOURCES += http/mythhttpcache.cpp
SOURCES += http/mythhttpservice.cpp
SOURCES += http/mythhttpmetaservice.cpp
SOURCES += http/mythhttpmetamethod.cpp
SOURCES += http/mythhttpservices.cpp
SOURCES += http/mythmimedatabase.cpp
SOURCES += http/mythmimetype.cpp
SOURCES += http/mythwsdl.cpp
SOURCES += http/mythxsd.cpp
SOURCES += http/serialisers/mythserialiser.cpp
SOURCES += http/serialisers/mythjsonserialiser.cpp
SOURCES += http/serialisers/mythxmlserialiser.cpp
SOURCES += http/serialisers/mythxmlplistserialiser.cpp
SOURCES += http/serialisers/mythcborserialiser.cpp

using_qtdbus {
    QT      += dbus
    HEADERS += platforms/mythpowerdbus.h
    SOURCES += platforms/mythpowerdbus.cpp
}

unix {
    SOURCES += mythsystemunix.cpp
    HEADERS += mythsystemunix.h
}

mingw | win32-msvc* {
    SOURCES += mythsystemwindows.cpp
    HEADERS += mythsystemwindows.h
    LIBS += -lzip
}

# Install headers to same location as libmyth to make things easier
inc.path = $${PREFIX}/include/mythtv/libmythbase
inc.files += mythdbcon.h mythdbparams.h mythbaseexp.h mythdb.h
inc.files += compat.h mythversion.h version.h
inc.files += mythobservable.h mythevent.h verbosedefs.h
inc.files += mythtimer.h lcddevice.h exitcodes.h mythdirs.h mythstorage.h
inc.files += mythsocket.h mythsocket_cb.h mythlogging.h
inc.files += mythcorecontext.h mythsystem.h storagegroup.h loggingserver.h
inc.files += mythlocale.h mythdownloadmanager.h
inc.files += mythtranslation.h iso639.h iso3166.h mythmedia.h mythmiscutil.h
inc.files += mythcdrom.h autodeletedeque.h dbutil.h mythdeque.h
inc.files += referencecounter.h referencecounterlist.h mythcommandlineparser.h
inc.files += mthread.h mthreadpool.h mythchrono.h
inc.files += filesysteminfo.h hardwareprofile.h bonjourregister.h serverpool.h
inc.files += plist.h signalhandling.h mythdate.h
inc.files += mythplugin.h mythpluginapi.h
inc.files += mythpluginexport.h
inc.files += remotefile.h mythsystemlegacy.h mythtypes.h
inc.files += threadedfilewriter.h mythsingledownload.h mythsession.h
inc.files += mythsorthelper.h mythdbcheck.h
inc.files += mythrandom.h
inc.files += netgrabbermanager.h
inc.files += netutils.h
inc.files += programinfo.h
inc.files += programtypes.h
inc.files += programtypeflags.h
inc.files += recordingstatus.h
inc.files += recordingtypes.h
inc.files += remoteutil.h
inc.files += rssmanager.h
inc.files += rssparse.h
inc.files += stringutil.h
inc.files += unziputil.h
inc.files += sizetliteral.h

inc2.path = $${PREFIX}/include/mythtv
inc2.files += mythconfig.h mythconfig.mak

INSTALLS += inc inc2

INCLUDEPATH += ..
DEPENDPATH  +=  ../../external/libudfread

DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES += LIBDIRNAME=\\\"$${LIBDIRNAME}\\\"
DEFINES += MBASE_API

macx {
    HEADERS += mythcdrom-darwin.h
    SOURCES += mythcdrom-darwin.cpp
    HEADERS += platforms/mythpowerosx.h
    SOURCES += platforms/mythpowerosx.cpp
    QMAKE_OBJECTIVE_CFLAGS += $$QMAKE_CXXFLAGS
    QMAKE_OBJECTIVE_CXXFLAGS += $$QMAKE_CXXFLAGS
    OBJECTIVE_HEADERS += platforms/mythcocoautils.h
    OBJECTIVE_SOURCES += platforms/mythcocoautils.mm
    LIBS              += -framework Cocoa -framework IOKit
}

linux {
    !android {
    SOURCES += mythcdrom-linux.cpp
    HEADERS += mythcdrom-linux.h
    }
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
    HEADERS += bonjourregister.h
    SOURCES += bonjourregister.cpp
    !macx: LIBS += -ldns_sd
}

mingw:LIBS += -lws2_32 -lz

win32-msvc* {

    LIBS += -lws2_32
    EXTRA_LIBS += -lzlib

    # we need to make sure version.h is generated.

    versionTarget.target  = version.h
    versionTarget.depends = FORCE
    versionTarget.commands = powershell -noprofile -executionpolicy bypass -File ../../version.ps1 ../..

    PRE_TARGETDEPS += version.h
    QMAKE_EXTRA_TARGETS += versionTarget
}

QT += xml sql network widgets

include ( ../libs-targetfix.pro )

using_system_libudfread: {
    DEFINES += HAVE_LIBUDFREAD
    QMAKE_CXXFLAGS += $$LIBUDFREAD_CFLAGS
    LIBS           += $$LIBUDFREAD_LIBS
} else {
    INCLUDEPATH += ../../external/libudfread
    DEPENDPATH  += ../../external/libudfread
    LIBS        += -L../../external/libudfread -lmythudfread-$$LIBVERSION
}

LIBS += $$EXTRA_LIBS $$LATE_LIBS

test_clean.commands = -cd test/ && $(MAKE) -f Makefile clean
clean.depends = test_clean
QMAKE_EXTRA_TARGETS += test_clean clean
test_distclean.commands = -cd test/ && $(MAKE) -f Makefile distclean
distclean.depends = test_distclean
QMAKE_EXTRA_TARGETS += test_distclean distclean
