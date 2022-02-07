#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Declare the plugin
#
add_library(
  mythgame
  gamedbcheck.cpp
  gamedbcheck.h
  gamedetails.cpp
  gamedetails.h
  gamehandler.cpp
  gamehandler.h
  gamescan.cpp
  gamescan.h
  gamesettings.cpp
  gamesettings.h
  gameui.cpp
  gameui.h
  mythgame.cpp
  rom_metadata.cpp
  rom_metadata.h
  romedit.cpp
  romedit.h
  rominfo.cpp
  rominfo.h)

#
# All remaining target information
#

target_compile_definitions(mythgame PRIVATE MPLUGIN_API NOUNCRYPT)

target_include_directories(
  mythgame PRIVATE . $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                   ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mythgame
  PRIVATE Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Widgets
          Qt${QT_VERSION_MAJOR}::Xml
          ${BASE_MYTHTV_LIBS}
          ${MYTHMETADATA}
          # The following should flow through from the include of mythbase, but
          # that doesn't seem bee working on an OSX compile or an android cross
          # compile.
          PkgConfig::LIBZIP
          ZLIB::ZLIB)

#
# Installation section
#

install(TARGETS mythgame
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/mythtv/plugins)

install(
  DIRECTORY scripts/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/metadata/Game
  PATTERN
    giantbomb.py
    PERMISSIONS
      OWNER_READ
      OWNER_WRITE
      OWNER_EXECUTE
      GROUP_READ
      GROUP_EXECUTE
      WORLD_READ
      WORLD_EXECUTE)
