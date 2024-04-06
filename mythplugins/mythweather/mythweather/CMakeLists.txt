#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Declare the plugin
#
add_library(
  mythweather
  mythweather.cpp
  sourceManager.cpp
  sourceManager.h
  weather.cpp
  weather.h
  weatherdbcheck.cpp
  weatherdbcheck.h
  weatherScreen.cpp
  weatherScreen.h
  weatherSetup.cpp
  weatherSetup.h
  weatherSource.cpp
  weatherSource.h
  weatherUtils.cpp
  weatherUtils.h)

#
# All remaining target information
#

target_compile_definitions(mythweather PRIVATE MPLUGIN_API)

target_include_directories(
  mythweather PRIVATE . $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                      ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mythweather
  PRIVATE Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Xml
          ${BASE_MYTHTV_LIBS} ${MYTHMETADATA})

#
# Installation section
#

install(TARGETS mythweather
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/mythtv/plugins)

install(FILES weather-screens.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/mythweather)

install(
  DIRECTORY scripts
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/mythweather
  PATTERN Make* EXCLUDE
  PATTERN README EXCLUDE
  PATTERN LICENSE EXCLUDE
  PATTERN .gitignore EXCLUDE)
