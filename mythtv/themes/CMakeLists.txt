#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

install(FILES mythuitheme.dtd
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/themes)

install(DIRECTORY fonts DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv)

install(
  DIRECTORY default
            default-wide
            classic
            DVR
            Slave
            Terra
            defaultmenu
            mediacentermenu
            MythCenter
            MythCenter-wide
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/themes)
