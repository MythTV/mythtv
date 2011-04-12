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

HEADERS += httprequest.h upnp.h ssdp.h taskqueue.h
HEADERS += upnpdevice.h upnptasknotify.h upnptasksearch.h threadpool.h upnputil.h
HEADERS += httpserver.h upnpcds.h upnpcdsobjects.h bufferedsocketdevice.h upnpmsrr.h
HEADERS += eventing.h upnpcmgr.h upnptaskevent.h upnptaskcache.h ssdpcache.h
HEADERS += upnpimpl.h multicast.h broadcast.h configuration.h
HEADERS += soapclient.h mythxmlclient.h mmembuf.h upnpexp.h
HEADERS += servicehost.h wsdl.h htmlserver.h serverSideScripting.h

HEADERS += serializers/serializer.h     serializers/xmlSerializer.h 
HEADERS += serializers/jsonSerializer.h serializers/soapSerializer.h

SOURCES += httprequest.cpp upnp.cpp ssdp.cpp taskqueue.cpp upnputil.cpp
SOURCES += upnpdevice.cpp upnptasknotify.cpp upnptasksearch.cpp threadpool.cpp
SOURCES += httpserver.cpp upnpcds.cpp upnpcdsobjects.cpp bufferedsocketdevice.cpp
SOURCES += eventing.cpp upnpcmgr.cpp upnpmsrr.cpp upnptaskevent.cpp ssdpcache.cpp
SOURCES += configuration.cpp soapclient.cpp mythxmlclient.cpp mmembuf.cpp
SOURCES += multicast.cpp htmlserver.cpp serverSideScripting.cpp
SOURCES += servicehost.cpp wsdl.cpp

SOURCES += serializers/serializer.cpp     serializers/xmlSerializer.cpp
SOURCES += serializers/jsonSerializer.cpp 

INCLUDEPATH += ../libmythbase ../libmythservicecontracts ..
INCLUDEPATH += ./serializers

DEPENDPATH  += ../libmythbase ..
LIBS      += -L../libmythbase -lmythbase-$$LIBVERSION
LIBS      += -L../libmythservicecontracts -lmythservicecontracts-$$LIBVERSION

LIBS += $$EXTRA_LIBS

mingw {

    LIBS += -lws2_32
}

inc.path = $${PREFIX}/include/mythtv/upnp/

inc.files  = httprequest.h upnp.h ssdp.h taskqueue.h bufferedsocketdevice.h
inc.files += upnpdevice.h upnptasknotify.h upnptasksearch.h threadpool.h upnputil.h
inc.files += httpserver.h httpstatus.h upnpcds.h upnpcdsobjects.h
inc.files += eventing.h upnpcmgr.h upnptaskevent.h upnptaskcache.h ssdpcache.h
inc.files += upnpimpl.h multicast.h broadcast.h configuration.h
inc.files += soapclient.h mythxmlclient.h mmembuf.h 
inc.files += servicehost.h wsdl.h htmlserver.h serverSideScripting.h

inc.files += serializers/serializer.h     serializers/xmlSerializer.h 
inc.files += serializers/jsonSerializer.h serializers/soapSerializer.h

INSTALLS += inc

macx {

    QMAKE_LFLAGS_SHLIB += -flat_namespace
}

QT += network xml sql script

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$LATE_LIBS
