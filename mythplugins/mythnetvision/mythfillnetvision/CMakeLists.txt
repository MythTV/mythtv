#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Declare the application
#
add_executable(mythfillnetvision mythfillnetvision.cpp)

#
# All remaining target information
#

target_include_directories(mythfillnetvision
                           PRIVATE . ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mythfillnetvision
  PRIVATE ${BASE_MYTHTV_LIBS} Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Widgets
          Qt${QT_VERSION_MAJOR}::Xml ${MYTHTV})

#
# Installation section
#

install(TARGETS mythfillnetvision RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
