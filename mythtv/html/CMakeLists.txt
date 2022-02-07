#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# mythfrontend.html is just a copy of frontend_index.qsp

install(
  FILES backend_index.qsp
        favicon.ico
        frontend_index.qsp
        menu.qsp
        mythbackend.html
        mythfrontend.html
        robots.txt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/html)

install(
  DIRECTORY 3rdParty
            apps
            assets
            css
            debug
            images
            js
            misc
            samples
            setup
            tv
            video
            xslt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/html)
