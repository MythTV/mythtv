#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Declare the plugin
#
add_library(
  mytharchive
  archivedbcheck.cpp
  archivedbcheck.h
  archivesettings.cpp
  archivesettings.h
  archiveutil.cpp
  archiveutil.h
  editmetadata.cpp
  editmetadata.h
  exportnative.cpp
  exportnative.h
  fileselector.cpp
  fileselector.h
  importnative.cpp
  importnative.h
  logviewer.cpp
  logviewer.h
  mytharchive.cpp
  mythburn.cpp
  mythburn.h
  recordingselector.cpp
  recordingselector.h
  selectdestination.cpp
  selectdestination.h
  themeselector.cpp
  themeselector.h
  thumbfinder.cpp
  thumbfinder.h
  videoselector.cpp
  videoselector.h)

#
# All remaining target information
#

target_include_directories(
  mytharchive PRIVATE . $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                      ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mytharchive
  PRIVATE Qt${QT_VERSION_MAJOR}::Core
          Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Widgets
          Qt${QT_VERSION_MAJOR}::Xml
          ${BASE_MYTHTV_LIBS}
          ${MYTHMETADATA}
          ${MYTHTV}
          PkgConfig::LIBAVCODEC
          PkgConfig::LIBAVFORMAT
          PkgConfig::LIBAVUTIL
          PkgConfig::LIBSWRESAMPLE
          PkgConfig::LIBSWSCALE)

#
# Installation section
#

install(TARGETS mytharchive
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/mythtv/plugins)
