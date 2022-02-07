#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

install(
  DIRECTORY theme/default/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/themes/default
  FILES_MATCHING
  PATTERN *.xml
  PATTERN *.png
  REGEX "(images|icons)" EXCLUDE)

install(
  DIRECTORY theme/default/images/ theme/default/icons/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/themes/default
  FILES_MATCHING
  PATTERN *.png)

install(
  DIRECTORY theme/default-wide/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/themes/default-wide
  FILES_MATCHING
  PATTERN *.xml
  PATTERN *.png
  REGEX "(images|icons)" EXCLUDE)

install(
  DIRECTORY theme/default-wide/images/ theme/default-wide/icons/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/themes/default-wide
  FILES_MATCHING
  PATTERN *.png)

install(
  DIRECTORY theme/menus/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv
  FILES_MATCHING
  PATTERN *.xml)
