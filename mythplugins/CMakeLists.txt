#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Can use cmake features introduced in 3.20.  Super-project has already
# validated the higher version required for android.
#
cmake_minimum_required(VERSION 3.20)

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
# Require the C++17 standard, and allow compiler extensions.
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

#
# Set CMake features
#
include(SetCmakePolicies NO_POLICY_SCOPE)
set(BUILD_SHARED_LIBS YES)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#
# Read user options (part 1).  On android, several of these options affect the
# initialization performed by the project function.
#
include(MythOptions)
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES1}")
include(${MYTH_USER_OVERRIDES1} OPTIONAL)

#
# Declare the MythTV plugins project
#
project(
  Plugins
  VERSION ${SUPER_VERSION}
  LANGUAGES C CXX
  DESCRIPTION "Additional plugins for the MythTV frontend."
  HOMEPAGE_URL https://www.mythtv.org)
include(VersionInformation)

#
# Read user options (part 2)
#
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES2}")
include(${MYTH_USER_OVERRIDES2} OPTIONAL)

#
# Inject code from cmake provided modules
#
include(CMakePushCheckState)
include(CheckTypeSize)
include(GNUInstallDirs)

#
# Set search paths
#
include(SetSearchPaths)

#
# Perform platform specific customizations.
#
if(CMAKE_CROSSCOMPILING)
  set(_CROSS "Cross")
endif()
include(platform/${CMAKE_SYSTEM_NAME}${_CROSS}Customization OPTIONAL)

include(UpdatePkgConfig)

#
# Set up for static analysis
#
include(StaticAnalysis)

#
# Check compiler and installed header files
#
include(CompilerCaching)
include(SetCompilerOptions)

# Debian policy requires that shared libraries be installed without executable
# permission.  Fedora policy requires that shared libraries be installed with
# the executable permission.
#
# Tell CMake to ignore the Debian policy and set the executable bits on all
# libraries.
set(CMAKE_INSTALL_SO_NO_EXE OFF)

#
# Find our dependancies
#
message(VERBOSE "including MythFindFFmpeg")
include(MythFindFFmpeg)
message(VERBOSE "including MythFindQt")
include(MythFindQt)
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=${QT_MIN_VERSION_HEX})

# Qt changes target build names. Reset them.
include(ResetTargetSuffixes)
reset_target_suffixes(FALSE)

#
# Android uses a different library name prefix for the plugins.
#
if(ANDROID)
  set(CMAKE_SHARED_LIBRARY_PREFIX_C "libmythplugin")
  set(CMAKE_SHARED_LIBRARY_PREFIX_CXX "libmythplugin")
  set(CMAKE_SHARED_MODULE_PREFIX_C "libmythplugin")
  set(CMAKE_SHARED_MODULE_PREFIX_CXX "libmythplugin")
endif()

#
# Find all the libs created by installed external projects
#
link_directories(${CMAKE_INSTALL_FULL_LIBDIR})
if(NOT LIBS_INSTALL_PREFIX STREQUAL CMAKE_INSTALL_PREFIX)
  link_directories(${LIBS_INSTALL_PREFIX}/lib)
endif()

#
# Find the mythtv modules built by the previous external package.
#
message(VERBOSE "including MythPluginsFindPackages")
include(MythPluginsFindPackages)
message(VERBOSE "including MythPluginsFindMythTV")
include(MythPluginsFindMythTV)

#
# Base set of MythTV libs that all plugins link against. This is needed for OSX
# linking of the plugins.
#
set(BASE_MYTHTV_LIBS ${MYTHBASE} ${MYTH} ${MYTHUI})

#
# Options that apply to all subdirectories
#
add_compile_definitions(MPLUGIN_API)

#
# Include all the plugin specific directories.
#
add_subdirectory(mytharchive)
add_subdirectory(mythbrowser)
add_subdirectory(mythgame)
add_subdirectory(mythmusic)
if(ENABLE_NETVISION)
  add_subdirectory(mythnetvision)
endif()
add_subdirectory(mythnews)
if(ENABLE_WEATHER)
  add_subdirectory(mythweather)
endif()
add_subdirectory(mythzoneminder)

#
# Consolidate setting common plugin properties. These must be individually
# tested for existence because they arent always compiled.
#
foreach(
  _target IN
  ITEMS mytharchive
        mythbrowser
        mythgame
        mythmusic
        mythnetvision
        mythnews
        mythweather
        mythzoneminder)
  if(TARGET ${_target})
    set_target_properties(
      PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}
                 VERSION
                 ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR})
  endif()
endforeach()
