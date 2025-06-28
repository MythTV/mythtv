#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Enumerate all headers
#
set(LIBMYTH_HEADERS
    mythaverror.h
    mythavframe.h
    mythcontext.h
    mythexp.h
    )

set(LIBMYTH_HEADERS_NOT_INSTALLED backendselect.h)

#
# Declare the library
#
add_library(
  myth
  # Include headers here so the they will be scanned for Qt classes
  # and potentially have MOC invoked on them.
  ${LIBMYTH_HEADERS_NOT_INSTALLED}
  ${LIBMYTH_HEADERS}
  backendselect.cpp
  mythaverror.cpp
  mythcontext.cpp
  )

#
# All remaining target information
#

target_compile_definitions(myth PRIVATE MYTH_API)

target_include_directories(
  myth
  PRIVATE .
  PUBLIC ${PROJECT_SOURCE_DIR}/libs)

target_link_libraries(
  myth
  PRIVATE Qt${QT_VERSION_MAJOR}::Core
          Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Xml
          Qt${QT_VERSION_MAJOR}::Widgets
  PUBLIC mythbase
         mythui
         mythupnp
         mythbluray
         PkgConfig::LIBAVCODEC
         PkgConfig::LIBAVFORMAT
         PkgConfig::LIBAVUTIL
         PkgConfig::LIBSWRESAMPLE)

#
# Installation section
#

install(TARGETS myth LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${LIBMYTH_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmyth)
