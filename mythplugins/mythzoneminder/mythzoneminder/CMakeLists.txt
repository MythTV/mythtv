#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Declare the plugin
#
add_library(
  mythzoneminder
  alarmnotifythread.cpp
  alarmnotifythread.h
  mythzoneminder.cpp
  zmclient.cpp
  zmclient.h
  zmconsole.cpp
  zmconsole.h
  zmdefines.h
  zmevents.cpp
  zmevents.h
  zmliveplayer.cpp
  zmliveplayer.h
  zmminiplayer.cpp
  zmminiplayer.h
  zmplayer.cpp
  zmplayer.h
  zmsettings.cpp
  zmsettings.h)

#
# All remaining target information
#

target_include_directories(
  mythzoneminder PRIVATE . $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                         ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mythzoneminder
  PRIVATE Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Widgets ${BASE_MYTHTV_LIBS})

#
# Installation section
#

install(TARGETS mythzoneminder
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/mythtv/plugins)
