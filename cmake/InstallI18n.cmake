#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

install(
  DIRECTORY i18n/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/i18n
  FILES_MATCHING
  PATTERN *.qm)
