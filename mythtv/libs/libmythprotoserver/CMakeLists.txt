set(HEADERS_PROTOSERVER mythprotoserverexp.h mythsocketmanager.h
                        sockethandler.h socketrequesthandler.h)

set(HEADERS_REQUESTHANDLER
    requesthandler/basehandler.h requesthandler/deletethread.h
    requesthandler/fileserverhandler.h requesthandler/fileserverutil.h
    requesthandler/messagehandler.h requesthandler/outboundhandler.h)

set(HEADERS_SOCKETHANDLER sockethandler/filetransfer.h)

add_library(
  mythprotoserver
  ${HEADERS_PROTOSERVER}
  ${HEADERS_REQUESTHANDLER}
  ${HEADERS_SOCKETHANDLER}
  mythsocketmanager.cpp
  requesthandler/basehandler.cpp
  requesthandler/deletethread.cpp
  requesthandler/fileserverhandler.cpp
  requesthandler/fileserverutil.cpp
  requesthandler/messagehandler.cpp
  requesthandler/outboundhandler.cpp
  sockethandler.cpp
  sockethandler/filetransfer.cpp)

target_compile_definitions(mythprotoserver PRIVATE PROTOSERVER_API)

target_include_directories(
  mythprotoserver
  PRIVATE .
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(
  mythprotoserver
  PUBLIC Qt${QT_VERSION_MAJOR}::Core
         Qt${QT_VERSION_MAJOR}::Network
         Qt${QT_VERSION_MAJOR}::Sql
         Qt${QT_VERSION_MAJOR}::Xml
         Qt${QT_VERSION_MAJOR}::Widgets
         mythbase
         mythui
         mythtv)

install(TARGETS mythprotoserver LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${HEADERS_PROTOSERVER}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythprotoserver)
install(
  FILES ${HEADERS_REQUESTHANDLER}
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythprotoserver/requesthandler)
install(
  FILES ${HEADERS_SOCKETHANDLER}
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythprotoserver/sockethandler)
