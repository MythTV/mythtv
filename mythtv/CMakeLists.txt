#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Catch running cmake in the wrong directory.
#
if(NOT SUPER_VERSION)
  message(FATAL_ERROR "You must run cmake from the parent of this directory.")
endif()

#
# Can use cmake features introduced in 3.20.  Super-project has already
# validated the higher version required for android.
#
cmake_minimum_required(VERSION 3.20)

#
# Validate parameters
#
foreach(_param IN ITEMS SUPER_SOURCE_DIR SUPER_VERSION MYTHTV_SOURCE_VERSION
                        MYTHTV_SOURCE_BRANCH)
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
# Declare the main MythTV project
#
project(
  MythTV
  VERSION ${SUPER_VERSION}
  LANGUAGES C CXX
  DESCRIPTION
    "MythTV is a Free Open Source software digital video recorder (DVR) project."
  HOMEPAGE_URL https://www.mythtv.org)
include(VersionInformation)

#
# Read user options (part 2)
#
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES2}")
include(${MYTH_USER_OVERRIDES2} OPTIONAL)
if(NOT MYTH_RUN_PREFIX)
  set(MYTH_RUN_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()

#
# Inject code from cmake provided modules
#
include(CMakePushCheckState)
include(CTest)
list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
include(CheckTypeSize)
include(GNUInstallDirs)

#
# Inject code from mythtv provided modules
#
include(GetLinuxInfo)
include(FedoraGlsLangHack)

#
# Find/Print host system information. LSB information used in configuring the
# libraries required by Vulkan support.
#
include("BuildSystem${CMAKE_HOST_SYSTEM_NAME}" OPTIONAL)

# For debugging
include(CMakePrintHelpers)

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
# Find required commands.
#
# Make is required by the python bindings.
#
find_program(MAKE_EXECUTABLE NAMES gmake make REQUIRED)
message(STATUS "Found ${MAKE_EXECUTABLE}")

#
# Set up for static analysis
#
include(StaticAnalysis)

#
# Check compiler and installed header files
#
include(CompilerCaching)
include(SetCompilerOptions)
include(MythCheckIncludes)
include(MythFindFFmpeg)
include(MythFindQt)
include(CheckExternalLibraries)
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=${QT_MIN_VERSION_HEX})

# Debian policy requires that shared libraries be installed without executable
# permission.  Fedora policy requires that shared libraries be installed with
# the executable permission.
#
# Tell CMake to ignore the Debian policy and set the executable bits on all
# libraries.
set(CMAKE_INSTALL_SO_NO_EXE OFF)

# Qt changes target build names. Reset them.
include(ResetTargetSuffixes)
reset_target_suffixes(${MYTH_VERSIONED_EXTENSIONS})

include(MythFindPackages)
include(PrintProperties)

#
# These directories should each have their own config.h file.  These are mainly
# external code, with some small customizations to integrate it into MythTV.
# Note: directories under external that contain no customization have already
# been built by the superpackage.
#
add_subdirectory(external)

#
# Allow all subsequently included directories to find the MythTV (myth)config.h
# file.
#
include_directories(${PROJECT_BINARY_DIR})

#
# Find all the libs created by installed external projects
#
link_directories(${CMAKE_INSTALL_FULL_LIBDIR})
if(NOT LIBS_INSTALL_PREFIX STREQUAL CMAKE_INSTALL_PREFIX)
  link_directories(${LIBS_INSTALL_PREFIX}/lib)
endif()

# FreeBSD hack. Make sure that the include directories populated by the
# superpackage show up before FreeBSD's default /usr/local directory.  This
# ensures that the proper versions of ffmpeg and exiv2 will be found while
# building MythTV.
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  include_directories(BEFORE ${CMAKE_INSTALL_FULL_INCLUDEDIR})
  include_directories(BEFORE ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)
endif()

#
# Debugging
#
if(0)
  include(DumpAllVariables)
  dump_cmake_variables()
endif()

#
# Finish the MYTH_BUILD_CONFIG variable here. It will be written to a file when
# the libs/libmythbase directory is processed.
#
add_build_config(ENABLE_BACKEND "backend")
add_build_config(ENABLE_FRONTEND "frontend")
list(
  SORT MYTHTV_BUILD_CONFIG_LIST
  COMPARE STRING
  CASE INSENSITIVE)
list(PREPEND MYTHTV_BUILD_CONFIG_LIST ${CMAKE_SYSTEM_NAME} ${CMAKE_BUILD_TYPE})
list(REMOVE_DUPLICATES MYTHTV_BUILD_CONFIG_LIST)
list(JOIN MYTHTV_BUILD_CONFIG_LIST " " MYTH_BUILD_CONFIG)
string(TOLOWER ${MYTH_BUILD_CONFIG} MYTH_BUILD_CONFIG)

# Setup run path (RPATH) support.  This tells the loader where to look for
# libraries.  Setting the paths relative to $ORIGIN (or @loader_path on OSX)
# tells the loader to look in the directory containing the executable, so the
# below paths will look in <blah>/bin, <blah>/lib, and <blah>/lib64 to find
# library files.
if(APPLE)
  set(base @loader_path)
else()
  set(base $ORIGIN)
endif()
set(CMAKE_INSTALL_RPATH ${base} ${base}/../lib ${base}/../lib64)

#
# Check the ENABLE_FRONTEND/BACKEND options.
#
if(ENABLE_FRONTEND)
  set(CONFIG_FRONTEND TRUE)
else()
  message(
    WARNING
      "Disabling the frontend is only intended to aid porting.  THIS WILL BREAK THINGS.  You are on your own now."
  )
endif()

if(ENABLE_BACKEND)
  set(CONFIG_BACKEND TRUE)
  # The CONFIG_IPTV flag should always be set when building a backend.
  set(CONFIG_IPTV TRUE)
else()
  message(
    WARNING
      "Disabling the backend is only intended to aid porting.  THIS WILL BREAK THINGS.  You are on your own now."
  )
endif()

#
# Need to build libmythmpeg2 on all platforms.  The original "configure" build
# system has an exception for windows builds, which use an external mpeg2
# library.
#
set(CONFIG_LIBMPEG2EXTERNAL OFF)

#
# Include all the MythTV specific directories.
#
add_subdirectory(docs)
add_subdirectory(html)
include(InstallI18n)
add_subdirectory(libs)
add_subdirectory(locales)
add_subdirectory(programs)
add_subdirectory(themes)

# Include the bindings directory at the end because it needs to add itself to an
# existing target. Dong so ensures that all the necessary python/perl setup
# processing occurs prior to trying to install the fines.
add_subdirectory(bindings)

#
# Consolidate setting common library properties
#
set_target_properties(
  myth
  mythbase
  mythdvdnav
  mythfreemheg
  mythmetadata
  mythmpeg2
  mythprotoserver
  mythtv
  mythui
  mythupnp
  PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}
             VERSION
             ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR})

#
# Dump build status information, ala the "configure" script.
#
include(PrintConfig)
