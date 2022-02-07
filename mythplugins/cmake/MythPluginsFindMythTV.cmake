#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# DEBUGGING - REMOVE ME
#
# include(CMakePrintSystemInformation)
include(CMakePrintHelpers)

#
# This module needs pkg-config functionality
#
find_package(PkgConfig REQUIRED)

#
# Find MythTV installed libraries.
#

find_library(
  MYTH
  NAMES myth myth-${CMAKE_PROJECT_VERSION_MAJOR}
  PATHS ${CMAKE_INSTALL_FULL_LIBDIR}
  NO_DEFAULT_PATH REQUIRED)
find_library(
  MYTHBASE
  NAMES mythbase mythbase-${CMAKE_PROJECT_VERSION_MAJOR}
  PATHS ${CMAKE_INSTALL_FULL_LIBDIR}
  NO_DEFAULT_PATH REQUIRED)
find_library(
  MYTHMETADATA
  NAMES mythmetadata mythmetadata-${CMAKE_PROJECT_VERSION_MAJOR}
  PATHS ${CMAKE_INSTALL_FULL_LIBDIR}
  NO_DEFAULT_PATH REQUIRED)
find_library(
  MYTHPROTOSERVER
  NAMES mythprotoserver mythprotoserver-${CMAKE_PROJECT_VERSION_MAJOR}
  PATHS ${CMAKE_INSTALL_FULL_LIBDIR}
  NO_DEFAULT_PATH REQUIRED)
find_library(
  MYTHTV
  NAMES mythtv mythtv-${CMAKE_PROJECT_VERSION_MAJOR}
  PATHS ${CMAKE_INSTALL_FULL_LIBDIR}
  NO_DEFAULT_PATH REQUIRED)
find_library(
  MYTHUI
  NAMES mythui mythui-${CMAKE_PROJECT_VERSION_MAJOR}
  PATHS ${CMAKE_INSTALL_FULL_LIBDIR}
  NO_DEFAULT_PATH REQUIRED)
find_library(
  MYTHUPNP
  NAMES mythupnp mythupnp-${CMAKE_PROJECT_VERSION_MAJOR}
  PATHS ${CMAKE_INSTALL_FULL_LIBDIR}
  NO_DEFAULT_PATH REQUIRED)
