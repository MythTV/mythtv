#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Declare the application
#
add_executable(
  mytharchivehelper
  ../mytharchive/archiveutil.cpp ../mytharchive/archiveutil.h
  ../mytharchive/remoteavformatcontext.h external/pxsup2dast.c
  external/pxsup2dast.h mytharchivehelper.cpp)

#
# All remaining target information
#

target_include_directories(mytharchivehelper
                           PRIVATE . ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mytharchivehelper
  PRIVATE Qt${QT_VERSION_MAJOR}::Core
          Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Widgets
          Qt${QT_VERSION_MAJOR}::Xml
          ${BASE_MYTHTV_LIBS}
          ${MYTHTV}
          PkgConfig::LIBAVFORMAT
          PkgConfig::LIBAVCODEC
          PkgConfig::LIBAVUTIL
          ZLIB::ZLIB)

#
# Installation section
#

install(TARGETS mytharchivehelper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
