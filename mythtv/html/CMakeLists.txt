#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# mythfrontend.html is just a copy of frontend_index.qsp

install(
  FILES
        favicon.ico
        mythbackend.html
        mythfrontend.html
        robots.txt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/html)

install(
  DIRECTORY 3rdParty
            apps
            assets
            images
            xslt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/html)
