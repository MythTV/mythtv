#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Declare the plugin
#
add_library(
  mythnews
  libmythnews.cpp
  mythnews.cpp
  mythnews.h
  mythnewsconfig.cpp
  mythnewsconfig.h
  mythnewseditor.cpp
  mythnewseditor.h
  newsarticle.cpp
  newsarticle.h
  newsdbcheck.cpp
  newsdbcheck.h
  newsdbutil.cpp
  newsdbutil.h
  newssite.cpp
  newssite.h)

#
# All remaining target information
#

target_include_directories(
  mythnews PRIVATE . $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                   ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mythnews
  PRIVATE Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Xml
          ${BASE_MYTHTV_LIBS})

#
# Installation section
#

install(TARGETS mythnews
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/mythtv/plugins)

install(FILES news-sites.xml
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/mythtv/mythnews)
