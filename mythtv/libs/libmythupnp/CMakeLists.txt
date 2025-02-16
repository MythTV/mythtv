#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Enumerate all headers
#

set(HEADERS_TO_INSTALL1
    bufferedsocketdevice.h
    eventing.h
    htmlserver.h
    httprequest.h
    httpserver.h
    mmembuf.h
    mythxmlclient.h
    serverSideScripting.h
    soapclient.h
    ssdp.h
    ssdpcache.h
    taskqueue.h
    upnp.h
    upnpcds.h
    upnpcdsobjects.h
    upnpcmgr.h
    upnpdevice.h
    upnphelpers.h
    upnpsubscription.h
    upnptaskevent.h
    upnptasknotify.h
    upnptasksearch.h
    upnputil.h
    )

set(HEADERS_TO_INSTALL2
    serializers/jsonSerializer.h serializers/serializer.h
    serializers/soapSerializer.h serializers/xmlSerializer.h)

#
# Declare the library
#
add_library(
  mythupnp
  bufferedsocketdevice.cpp
  bufferedsocketdevice.h
  eventing.cpp
  eventing.h
  htmlserver.cpp
  htmlserver.h
  httprequest.cpp
  httprequest.h
  httpserver.cpp
  httpserver.h
  mmembuf.cpp
  mmembuf.h
  mmulticastsocketdevice.cpp
  mmulticastsocketdevice.h
  msocketdevice.cpp
  msocketdevice.h
  mythxmlclient.cpp
  mythxmlclient.h
  serializers/jsonSerializer.cpp
  serializers/jsonSerializer.h
  serializers/serializer.cpp
  serializers/serializer.h
  serializers/soapSerializer.h
  serializers/xmlplistSerializer.cpp
  serializers/xmlplistSerializer.h
  serializers/xmlSerializer.cpp
  serializers/xmlSerializer.h
  soapclient.cpp
  soapclient.h
  ssdp.cpp
  ssdp.h
  ssdpcache.cpp
  ssdpcache.h
  taskqueue.cpp
  taskqueue.h
  upnp.cpp
  upnp.h
  upnpcds.cpp
  upnpcds.h
  upnpcdsobjects.cpp
  upnpcdsobjects.h
  upnpcmgr.cpp
  upnpcmgr.h
  upnpdevice.cpp
  upnpdevice.h
  upnpexp.h
  upnphelpers.cpp
  upnphelpers.h
  upnpmsrr.cpp
  upnpmsrr.h
  upnpserviceimpl.cpp
  upnpserviceimpl.h
  upnpsubscription.cpp
  upnpsubscription.h
  upnptaskevent.cpp
  upnptaskevent.h
  upnptasknotify.cpp
  upnptasknotify.h
  upnptasksearch.cpp
  upnptasksearch.h
  upnputil.cpp
  upnputil.h
  websocket.cpp
  websocket.h
  websocket_extensions/websocket_mythevent.cpp
  websocket_extensions/websocket_mythevent.h
  )

#
# All remaining target information
#

target_compile_definitions(mythupnp PRIVATE UPNP_API)

target_include_directories(
  mythupnp
  PRIVATE .
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(
  mythupnp
  PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network
         Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Xml mythbase
  )

#
# conditionally included code
#
if(UNIX)
  target_sources(mythupnp PRIVATE msocketdevice_unix.cpp)
elseif(WIN32)
  target_sources(mythupnp PRIVATE msocketdevice_win.cpp)
endif()

if(TARGET Qt${QT_VERSION_MAJOR}::Script)
  target_link_libraries(mythupnp PUBLIC Qt${QT_VERSION_MAJOR}::Script
                                        Qt${QT_VERSION_MAJOR}::ScriptTools)
  target_sources(mythupnp PRIVATE serverSideScripting.cpp serverSideScripting.h)
endif()

if(APPLE)
  # set(QMAKE_LFLAGS_SHLIB -flat_namespace)
endif()

#
# Installation section
#

install(TARGETS mythupnp LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${HEADERS_TO_INSTALL1}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythupnp)

install(FILES ${HEADERS_TO_INSTALL2}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythupnp/serializers)

install(FILES CDS_scpd.xml CMGR_scpd.xml MSRR_scpd.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv)
