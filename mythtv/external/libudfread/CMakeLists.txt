#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

cmake_minimum_required(VERSION 3.20)
project(
  libudfread
  VERSION 1.2.0
  LANGUAGES C)
set(BUILD_SHARED_LIBS YES)

# Inject code from cmake provided modules
include(CheckIncludeFile)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(pthread.h HAVE_PTHREAD_H)
check_include_file(unistd.h HAVE_UNISTD_H)

set(HEADERS blockinput.h udfread.h udfread-version.h)

add_library(udfread
  ${HEADERS}
  attributes.h
  default_blockinput.c
  default_blockinput.h
  ecma167.c
  ecma167.h
  udfread-version.c
  udfread.c
  )

# Create stripped down config.h with only the symbols that are used in this
# directory.
configure_file(config.h.in config.h @ONLY)

set_target_properties(
  udfread
  PROPERTIES COMPILE_DEFINITIONS "HAVE_CONFIG_H"
             VERSION ${libudfread_VERSION}
             SOVERSION ${libudfread_VERSION_MAJOR})

target_compile_definitions(udfread PRIVATE UDFREAD_API_EXPORT)

target_include_directories(
  udfread
  PUBLIC ${CMAKE_INSTALL_INCLUEDIR}/udfread
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

#
# Install libudfread
#
install(TARGETS udfread LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

#
# Install headers
#
install(FILES ${HEADERS} DESTINATION include/udfread)

#
# Create and install pkg-config file
#
set(VERSION ${libudfread_VERSION})
set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_FULL_BINDIR})
set(libdir ${CMAKE_INSTALL_FULL_LIBDIR})
set(includedir ${CMAKE_INSTALL_FULL_INCLUDEDIR})
configure_file(libudfread.pc.in libudfread.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/libudfread.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
