#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Declare the plugin
#
add_library(
  mythnetvision
  mythnetvision.cpp
  netbase.cpp
  netbase.h
  netcommon.cpp
  netcommon.h
  neteditorbase.cpp
  neteditorbase.h
  netsearch.cpp
  netsearch.h
  nettree.cpp
  nettree.h
  rsseditor.cpp
  rsseditor.h
  searcheditor.cpp
  searcheditor.h
  treeeditor.cpp
  treeeditor.h)

#
# All remaining target information
#

target_include_directories(
  mythnetvision PRIVATE . $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                        ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mythnetvision
  PRIVATE Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Xml
          ${BASE_MYTHTV_LIBS} ${MYTHMETADATA})

#
# Installation section
#

install(TARGETS mythnetvision
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/mythtv/plugins)

install(DIRECTORY icons
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/mythtv/mythnetvision)
