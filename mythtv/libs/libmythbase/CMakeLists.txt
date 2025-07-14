#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

#
# Enumerate all headers
#

set(LIBMYTHBASE_HEADERS
    autodeletedeque.h
    compat.h
    dbutil.h
    exitcodes.h
    filesysteminfo.h
    hardwareprofile.h
    iso3166.h
    iso639.h
    lcddevice.h
    loggingserver.h
    mthread.h
    mthreadpool.h
    mythbaseexp.h
    mythcdrom.h
    mythchrono.h
    mythcommandlineparser.h
    mythcorecontext.h
    mythdate.h
    mythdb.h
    mythdbcheck.h
    mythdbcon.h
    mythdbparams.h
    mythdeque.h
    mythdirs.h
    mythdownloadmanager.h
    mythevent.h
    mythlocale.h
    mythlogging.h
    mythmedia.h
    mythmiscutil.h
    mythobservable.h
    mythplugin.h
    mythpluginapi.h
    mythpluginexport.h
    mythrandom.h
    mythsession.h
    mythsingledownload.h
    mythsocket.h
    mythsocket_cb.h
    mythsorthelper.h
    mythstorage.h
    mythsystem.h
    mythsystemlegacy.h
    mythtimer.h
    mythtranslation.h
    mythtypes.h
    netgrabbermanager.h
    netutils.h
    programinfo.h
    programtypes.h
    programtypeflags.h
    recordingstatus.h
    recordingtypes.h
    referencecounter.h
    referencecounterlist.h
    remotefile.h
    remoteutil.h
    rssparse.h
    rssmanager.h
    serverpool.h
    signalhandling.h
    sizetliteral.h
    storagegroup.h
    stringutil.h
    threadedfilewriter.h
    verbosedefs.h)

set(LIBMYTHBASE_HEADERS_NOT_INSTALLED
    mythbinaryplist.h
    configuration.h
    housekeeper.h
    logging.h
    mconcurrent.h
    mythappname.h
    mythhdd.h
    mythpower.h
    mythscheduler.h
    mythsystemprivate.h
    mythtimezone.h
    portchecker.h
    programinfoupdater.h
    unzip2.h
    unziputil.h)

set(LIBMYTHBASE_HTTP_HEADERS
    http/mythhttpcache.h
    http/mythhttpcommon.h
    http/mythhttpdata.h
    http/mythhttpencoding.h
    http/mythhttpfile.h
    http/mythhttpinstance.h
    http/mythhttpmetamethod.h
    http/mythhttpmetaservice.h
    http/mythhttpparser.h
    http/mythhttpranges.h
    http/mythhttprequest.h
    http/mythhttpresponse.h
    http/mythhttprewrite.h
    http/mythhttproot.h
    http/mythhttps.h
    http/mythhttpserver.h
    http/mythhttpservice.h
    http/mythhttpservices.h
    http/mythhttpsocket.h
    http/mythhttpthread.h
    http/mythhttpthreadpool.h
    http/mythhttptypes.h
    http/mythmimedatabase.h
    http/mythmimetype.h
    http/mythwebsocket.h
    http/mythwebsocketevent.h
    http/mythwebsockettypes.h
    http/mythwsdl.h
    http/mythxsd.h
    http/serialisers/mythcborserialiser.h
    http/serialisers/mythjsonserialiser.h
    http/serialisers/mythserialiser.h
    http/serialisers/mythxmlplistserialiser.h
    http/serialisers/mythxmlserialiser.h)

#
# Declare the library
#
add_library(
  mythbase
  # Include headers here so the they will be scanned for Qt classes
  # and potentially have MOC invoked on them.
  ${LIBMYTHBASE_HEADERS}
  ${LIBMYTHBASE_HEADERS_NOT_INSTALLED}
  ${LIBMYTHBASE_HTTP_HEADERS}
  configuration.cpp
  dbcheckcommon.cpp
  dbutil.cpp
  filesysteminfo.cpp
  hardwareprofile.cpp
  housekeeper.cpp
  http/mythhttpcache.cpp
  http/mythhttpcommon.cpp
  http/mythhttpdata.cpp
  http/mythhttpencoding.cpp
  http/mythhttpfile.cpp
  http/mythhttpinstance.cpp
  http/mythhttpmetamethod.cpp
  http/mythhttpmetaservice.cpp
  http/mythhttpparser.cpp
  http/mythhttpranges.cpp
  http/mythhttprequest.cpp
  http/mythhttpresponse.cpp
  http/mythhttprewrite.cpp
  http/mythhttproot.cpp
  http/mythhttps.cpp
  http/mythhttpserver.cpp
  http/mythhttpservice.cpp
  http/mythhttpservices.cpp
  http/mythhttpsocket.cpp
  http/mythhttpthread.cpp
  http/mythhttpthreadpool.cpp
  http/mythmimedatabase.cpp
  http/mythmimetype.cpp
  http/mythwebsocket.cpp
  http/mythwebsocketevent.cpp
  http/mythwebsockettypes.cpp
  http/mythwsdl.cpp
  http/mythxsd.cpp
  http/serialisers/mythcborserialiser.cpp
  http/serialisers/mythjsonserialiser.cpp
  http/serialisers/mythserialiser.cpp
  http/serialisers/mythxmlplistserialiser.cpp
  http/serialisers/mythxmlserialiser.cpp
  iso3166.cpp
  iso639.cpp
  lcddevice.cpp
  logging.cpp
  loggingserver.cpp
  mthread.cpp
  mthreadpool.cpp
  mythbinaryplist.cpp
  mythcdrom.cpp
  mythcommandlineparser.cpp
  mythconfig.h.in
  mythcorecontext.cpp
  mythdate.cpp
  mythdb.cpp
  mythdbcon.cpp
  mythdbparams.cpp
  mythdirs.cpp
  mythdownloadmanager.cpp
  mythevent.cpp
  mythhdd.cpp
  mythlocale.cpp
  mythmedia.cpp
  mythmiscutil.cpp
  mythobservable.cpp
  mythplugin.cpp
  mythpower.cpp
  mythrandom.cpp
  mythsession.cpp
  mythsingledownload.cpp
  mythsocket.cpp
  mythsorthelper.cpp
  mythstorage.cpp
  mythsystem.cpp
  mythsystemlegacy.cpp
  mythtimer.cpp
  mythtimezone.cpp
  mythtranslation.cpp
  mythtypes.cpp
  mythversion.cpp
  mythversion.h.in
  netgrabbermanager.cpp
  netutils.cpp
  portchecker.cpp
  programinfo.cpp
  programinfoupdater.cpp
  programtypes.cpp
  recordingstatus.cpp
  recordingtypes.cpp
  referencecounter.cpp
  remotefile.cpp
  remoteutil.cpp
  rssparse.cpp
  rssmanager.cpp
  serverpool.cpp
  signalhandling.cpp
  storagegroup.cpp
  stringutil.cpp
  threadedfilewriter.cpp
  unzip2.cpp
  unziputil.cpp)

#
# All remaining target information
#

target_compile_definitions(mythbase PRIVATE MBASE_API)

# The libudfread library is guaranteed to be installed by this point. Either by
# the system, or by the higher level CMakeLists.txt file that includes MythTV as
# an external project.
set_source_files_properties(mythcdrom.cpp PROPERTIES COMPILE_DEFINITIONS
                                                     HAVE_LIBUDFREAD)

set_source_files_properties(
  mythdirs.cpp
  PROPERTIES
    COMPILE_DEFINITIONS
    "RUNPREFIX=\"${MYTH_RUN_PREFIX}\";LIBDIRNAME=\"${CMAKE_INSTALL_LIBDIR}\"")
target_include_directories(
  mythbase
  PRIVATE .
  PUBLIC # Includes with directory name
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
         # Generated includes with dir name
         $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/libs>
         # For mythconfig.h
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(
  mythbase
  PUBLIC Qt${QT_VERSION_MAJOR}::Core
         Qt${QT_VERSION_MAJOR}::Network
         Qt${QT_VERSION_MAJOR}::Sql
         Qt${QT_VERSION_MAJOR}::Widgets
         Qt${QT_VERSION_MAJOR}::Xml
         PkgConfig::LIBUDFREAD
         PkgConfig::LIBZIP
         $<TARGET_NAME_IF_EXISTS:Qt6::Core5Compat>
         $<TARGET_NAME_IF_EXISTS:X11::X11>
         $<TARGET_NAME_IF_EXISTS:Threads::Threads>
         ZLIB::ZLIB)
if(ANDROID)
  target_link_libraries(mythbase PRIVATE log)
elseif(WIN32)
  target_link_libraries(mythbase PUBLIC ssp wsock32)
endif()

#
# conditionally included code
#
if(UNIX)
  target_sources(mythbase PRIVATE mythsystemunix.h mythsystemunix.cpp)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_sources(mythbase PRIVATE mythcdrom-linux.h mythcdrom-linux.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_sources(mythbase PRIVATE mythcdrom-freebsd.h mythcdrom-freebsd.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  target_sources(
    mythbase
    PRIVATE mythcdrom-darwin.h mythcdrom-darwin.cpp platforms/mythpowerosx.h
            platforms/mythpowerosx.cpp platforms/mythcocoautils.h
            platforms/mythcocoautils.mm)
  target_link_libraries(
    mythbase PRIVATE ${APPLE_IOKIT_LIBRARY} ${APPLE_COCOA_LIBRARY}
                     ${APPLE_COREFOUNDATION_LIBRARY})
elseif(WIN32)
  target_sources(mythbase PRIVATE mythsystemwindows.h mythsystemwindows.cpp)
endif()

if(TARGET PkgConfig::LIBDNS_SD)
  target_sources(mythbase PRIVATE bonjourregister.h bonjourregister.cpp)
  target_link_libraries(mythbase PRIVATE PkgConfig::LIBDNS_SD)
endif()

if(TARGET Qt${QT_VERSION_MAJOR}::DBus)
  target_sources(mythbase PRIVATE platforms/mythpowerdbus.h
                                  platforms/mythpowerdbus.cpp)
  target_link_libraries(mythbase PRIVATE Qt${QT_VERSION_MAJOR}::DBus)
endif()

#
# Generated files section
#
configure_file(mythversion.h.in mythversion.h @ONLY)
configure_file(mythconfig.h.in mythconfig.h @ONLY)
file(CREATE_LINK ${CMAKE_CURRENT_BINARY_DIR}/mythconfig.h
     ${PROJECT_BINARY_DIR}/config.h SYMBOLIC)

#
# Regenerate version.h every compile
#
add_custom_target(
  update_version_h ALL
  COMMAND ${PROJECT_SOURCE_DIR}/version.sh $<IF:$<BOOL:${VERBOSE}>,,-q>
  ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}
  COMMENT "Update version.h"
  BYPRODUCTS version.h)

#
# Installation section
#

install(TARGETS mythbase LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mythconfig.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv)

install(FILES ${LIBMYTHBASE_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/version.h
              ${CMAKE_CURRENT_BINARY_DIR}/mythversion.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythbase)

if(ANDROID)
  set(DEST_PREFIX mythbase${CMAKE_SHARED_LIBRARY_SUFFIX_C})
  string(REPLACE ".so" "" DEST_PREFIX ${DEST_PREFIX})
  install(
    FILES android-dependencies.xml
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RENAME ${DEST_PREFIX}-android-dependencies.xml)
endif(ANDROID)
