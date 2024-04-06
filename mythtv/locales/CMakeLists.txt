#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# mythfrontend.html is just a copy of frontend_index.qsp

install(
  DIRECTORY .
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/locales
  FILES_MATCHING
  PATTERN "*.xml")
