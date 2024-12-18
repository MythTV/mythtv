#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Enumerate all headers
#

set(HEADERS serviceexp.h service.h datacontracthelper.h)

set(DATACONTRACTS_HEADERS
    datacontracts/enum.h
    datacontracts/enumItem.h
    datacontracts/frontendActionList.h
    datacontracts/frontendStatus.h
   )

set(SERVICES_HEADERS
    services/frontendServices.h
    services/rttiServices.h
   )

#
# Declare the library
#
add_library(mythservicecontracts ${DATACONTRACTS_HEADERS} ${HEADERS}
                                 ${SERVICES_HEADERS} service.cpp)

#
# All remaining target information
#

target_compile_definitions(mythservicecontracts PRIVATE SERVICE_API)

target_include_directories(
  mythservicecontracts
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(
  mythservicecontracts
  PUBLIC Qt${QT_VERSION_MAJOR}::Core
         $<$<BOOL:${USING_QTSCRIPT}>:Qt${QT_VERSION_MAJOR}::Script> mythbase)

#
# Installation section
#

install(TARGETS mythservicecontracts
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

#
# Copy three files in the libxxx directory, and then copy the entire contracts
# and services directories into the libxxx directory.
#
install(FILES ${HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythservicecontracts)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/datacontracts
                  ${CMAKE_CURRENT_SOURCE_DIR}/services
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythservicecontracts)
