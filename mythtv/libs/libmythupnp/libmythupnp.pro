include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythupnp-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
DEFINES += UPNP_API

setting.path = $${PREFIX}/share/mythtv/
setting.files += CDS_scpd.xml CMGR_scpd.xml MSRR_scpd.xml MXML_scpd.xml

INSTALLS += setting

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += version.cpp

# Input

HEADERS += mmulticastsocketdevice.h
HEADERS += msocketdevice.h
HEADERS += httprequest.h upnp.h ssdp.h taskqueue.h upnpsubscription.h
HEADERS += upnpdevice.h upnptasknotify.h upnptasksearch.h upnputil.h
HEADERS += httpserver.h upnpcds.h upnpcdsobjects.h bufferedsocketdevice.h upnpmsrr.h
HEADERS += eventing.h upnpcmgr.h upnptaskevent.h ssdpcache.h
HEADERS += soapclient.h mythxmlclient.h mmembuf.h upnpexp.h
HEADERS += upnpserviceimpl.h
HEADERS += htmlserver.h
HEADERS += upnphelpers.h websocket.h

HEADERS += serializers/serializer.h     serializers/xmlSerializer.h 
HEADERS += serializers/jsonSerializer.h serializers/soapSerializer.h
HEADERS += serializers/xmlplistSerializer.h

HEADERS += websocket_extensions/*.h

SOURCES += mmulticastsocketdevice.cpp
SOURCES += msocketdevice.cpp
unix:SOURCES += msocketdevice_unix.cpp
mingw | win32-msvc*:SOURCES += msocketdevice_win.cpp
SOURCES += httprequest.cpp upnp.cpp ssdp.cpp taskqueue.cpp upnputil.cpp
SOURCES += upnpdevice.cpp upnptasknotify.cpp upnptasksearch.cpp
SOURCES += httpserver.cpp upnpcds.cpp upnpcdsobjects.cpp bufferedsocketdevice.cpp
SOURCES += eventing.cpp upnpcmgr.cpp upnpmsrr.cpp upnptaskevent.cpp ssdpcache.cpp
SOURCES += soapclient.cpp mythxmlclient.cpp mmembuf.cpp
SOURCES += upnpserviceimpl.cpp
SOURCES += htmlserver.cpp
SOURCES += upnpsubscription.cpp
SOURCES += upnphelpers.cpp websocket.cpp

SOURCES += serializers/serializer.cpp     serializers/xmlSerializer.cpp
SOURCES += serializers/jsonSerializer.cpp 
SOURCES += serializers/xmlplistSerializer.cpp

SOURCES += websocket_extensions/*.cpp

using_qtscript: {
    HEADERS += serverSideScripting.h
    SOURCES += serverSideScripting.cpp
}

INCLUDEPATH += ..

LIBS      += -L../libmythbase -lmythbase-$$LIBVERSION

LIBS += $$EXTRA_LIBS

mingw | win32-msvc* {

    LIBS += -lws2_32
}

win32-msvc*:LIBS += -lzlib


inc.path = $${PREFIX}/include/mythtv/libmythupnp/

inc.files  = httprequest.h upnp.h ssdp.h taskqueue.h bufferedsocketdevice.h
inc.files += upnpdevice.h upnptasknotify.h upnptasksearch.h upnputil.h
inc.files += httpserver.h httpstatus.h upnpcds.h upnpcdsobjects.h
inc.files += eventing.h upnpcmgr.h upnptaskevent.h ssdpcache.h
inc.files += upnpimpl.h
inc.files += soapclient.h mythxmlclient.h mmembuf.h upnpsubscription.h
inc.files += htmlserver.h serverSideScripting.h
inc.files += upnphelpers.h

inc2.path = $${PREFIX}/include/mythtv/libmythupnp/serializers
inc2.files += serializers/serializer.h     serializers/xmlSerializer.h
inc2.files += serializers/jsonSerializer.h serializers/soapSerializer.h

INSTALLS += inc inc2

macx {

    QMAKE_LFLAGS_SHLIB += -flat_namespace
}

QT += network xml sql

using_qtscript: QT += script

mingw | win32-msvc* {

  # script debugger currently only enabled for WIN32 builds

  QT += scripttools

}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$LATE_LIBS
