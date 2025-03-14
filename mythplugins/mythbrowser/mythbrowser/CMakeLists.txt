#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

message(STATUS "MythBrowser requires QtWebkit (which is no  longer supported).")
return()

#
# Declare the plugin
#
add_library(
  mythbrowser
  bookmarkeditor.cpp
  bookmarkeditor.h
  bookmarkmanager.cpp
  bookmarkmanager.h
  browserdbutil.cpp
  browserdbutil.h
  libmythbrowser.cpp
  mythbrowser.cpp
  mythbrowser.h
  mythflashplayer.cpp
  mythflashplayer.h
  webpage.cpp
  webpage.h)

#
# All remaining target information
#

target_include_directories(
  mythbrowser PRIVATE . $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                      ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mythbrowser
  PRIVATE Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Widgets
          Qt${QT_VERSION_MAJOR}::Xml
          Qt${QT_VERSION_MAJOR}::Widgets
          ${BASE_MYTHTV_LIBS}
          # This don't seem to flow through on OSX.
          $<$<BOOL:${APPLE}>:${MYTHTV}>)

#
# Installation section
#

install(TARGETS mythbrowser
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/mythtv/plugins)
