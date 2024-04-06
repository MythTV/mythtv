#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Can use cmake features introduced in 3.16.  Super-project has already
# validated the higher version required for android.
#
cmake_minimum_required(VERSION 3.16)

#
# Validate parameters
#
foreach(_param IN ITEMS SUPER_SOURCE_DIR SUPER_VERSION)
  if(${_param} STREQUAL "")
    message(FATAL_ERROR "${_param} is a required parameter")
  endif()
endforeach()

if(NOT DEFINED LIBS_INSTALL_PREFIX OR LIBS_INSTALL_PREFIX STREQUAL "")
  set(LIBS_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()

#
# Where to find MythTV provided modules
#
# From the sources:
list(APPEND CMAKE_MODULE_PATH "${SUPER_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#
# Set CMake features
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(SetCmakePolicies NO_POLICY_SCOPE)
set(CMAKE_AUTOMOC ON)

#
# Read user options (part 1).  On android, several of these options affect the
# initialization performed by the project function.
#
include(MythOptions)
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES1}")
include(${MYTH_USER_OVERRIDES1} OPTIONAL)

#
# Declare the 'theme string tool' project
#
project(
  ThemeStringTool
  VERSION ${SUPER_VERSION}
  LANGUAGES CXX)
include(VersionInformation)

#
# Read user options (part 2)
#
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES2}")
include(${MYTH_USER_OVERRIDES2} OPTIONAL)

# Inject code from project provided modules
include(SetSearchPaths)
include(SetCompilerOptions)
include(MythFindQt)

add_executable(themestringtool themestrings.cpp)

target_link_libraries(
  themestringtool
  PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets
         Qt${QT_VERSION_MAJOR}::Xml)
