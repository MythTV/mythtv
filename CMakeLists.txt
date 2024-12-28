#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Set minimum required cmake version
#
# ~~~
# Supported CMake versions (as of 2023-10-25):
#   Ubuntu 20.04: 3.16.3 (not a target, 22.04 has 3.22.1)
#   Debian 11:    3.18.4 (12 has 3.25.1)
#   Centos 8/9:   3.20.2
#   RHEL 8/9:     3.20.2
#   SuSE 15.4:    3.20.4
#   Others:       3.22 or better
#
# Can use cmake features introduced in 3.16.
#
# Presets were introduced in 3.19 and would be helpful for wrangling
# compiler options.  3.19 also allows spaces in the names of tests.
# ~~~
cmake_minimum_required(VERSION 3.20)

#
# Where to find additional modules
#
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

#
# Determine which cmake variables came from the command line.
#
# This MUST be done before the call to 'project'.
#
get_cmake_property(vars CACHE_VARIABLES)
foreach(var IN LISTS vars)
  get_property(
    currentHelpString
    CACHE "${var}"
    PROPERTY HELPSTRING)
  if("${currentHelpString}" MATCHES
     "No help, variable specified on the command line.")
    list(APPEND CMDLINE_ARGS "-D${var}=${${var}}")
    # message("${var} = [${${var}}]  --  ${currentHelpString}")
  endif()
endforeach()

# Collapse all the various ways of specifying an android build down into a
# single flag.
if(ARM64
   OR SDK
   OR CMAKE_ANDROID_ARCH_ABI)
  set(ANDROID ON)
endif()

#
# Copied from cmake/Modules/CMakeGenericSystem.cmake.  That module isn't
# included on windows builds, and we need this variable set properly.
#
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT 1)
endif()

#
# Read user options (part 1).  On android, several of these options affect the
# initialization performed by the project function.
#
message(STATUS "Including default MythTV options")
include(MythOptions)
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES1}")
include(${MYTH_USER_OVERRIDES1} OPTIONAL)

#
# Initial platform customization before declaring project. (CMAKE_CROSSCOMPILING
# isn't set yet. Nor is ANDROID or WIN32. The former is set by the call to
# project, and the latter two are set by the next two includes.)
#
include(platform/AndroidCrossEarlyInitialization)
include(platform/WindowsCrossEarlyInitialization)

#
# Declare the superbuild.
#
# This is where the all the android toolchain bits get pulled in.
#
project(
  MythTV-SuperBuild
  VERSION 35.0.0
  DESCRIPTION
    "MythTV is a Free Open Source software digital video recorder (DVR) project."
  HOMEPAGE_URL https://www.mythtv.org
  LANGUAGES C CXX)
include(VersionInformation)

message(
  STATUS "Building on ${CMAKE_HOST_SYSTEM_PROCESSOR} ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "Building for ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_SYSTEM_NAME}")

# Get the mythtv default install path
if(CMAKE_CROSSCOMPILING)
  set(_CROSS "Cross")
endif()
include(platform/${CMAKE_SYSTEM_NAME}${_CROSS}DefaultPrefix OPTIONAL)

#
# Read user options (part 2)
#
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES2}")
include(${MYTH_USER_OVERRIDES2} OPTIONAL)

#
# Validate build system and build location.
#
include(platform/${CMAKE_SYSTEM_NAME}${_CROSS}ValidateBuildSystem OPTIONAL)
if(PROJECT_BINARY_DIR STREQUAL PROJECT_SOURCE_DIR)
  message(FATAL_ERROR "You may not compile in the source directory.")
endif()
if(NOT TARBALL_DIR)
  set(TARBALL_DIR "${PROJECT_SOURCE_DIR}/tarballs")
endif()
if(NOT JAVA_HOME)
  # Java home hasn't already been set by one of the platform build system
  # validations.
  #
  # (Have to put an environment variable in real variable to test it.)
  set(_TMP_ENV $ENV{JAVA_HOME})
  if(_TMP_ENV)
    message(STATUS "Using java from environment: ${_TMP_ENV}")
    set(JAVA_HOME ${_TMP_ENV})
  elseif(MYTH_JAVA_HOME)
    message(STATUS "Using java from options: ${MYTH_JAVA_HOME}")
    set(JAVA_HOME ${MYTH_JAVA_HOME})
  else()
    message(STATUS "Using java from system path")
  endif()
endif()

#
# FFmpeg doesn't support compiling out-of-tree after compiling in-tree. Catch
# this early and print an error message.
#
if(EXISTS ${PROJECT_SOURCE_DIR}/mythtv/external/FFmpeg/config.h)
  message(
    FATAL_ERROR
      "Detecting build artifacts in the source tree. You must clean the source directory before running cmake."
  )
endif()

#
# If the user didn't specify CMAKE_INSTALL_PREFIX on the command line, use the
# mythtv or user default value.
#
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND NOT MYTH_DEFAULT_PREFIX
                                                   STREQUAL "")
  set(CMAKE_INSTALL_PREFIX ${MYTH_DEFAULT_PREFIX})
endif()
if(NOT DEFINED CMAKE_INSTALL_PREFIX OR CMAKE_INSTALL_PREFIX STREQUAL "")
  message(FATAL_ERROR "The CMAKE_INSTALL_PREFIX variable is undefined.")
endif()
if(CMAKE_CROSSCOMPILING AND CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
  message(
    FATAL_ERROR "CMAKE_INSTALL_PREFIX is /usr/local while cross compiling.")
endif()

#
# If the user didn't specify LIBS_INSTALL_PREFIX on the command line, use the
# mythtv default value.
#
if(NOT DEFINED LIBS_INSTALL_PREFIX OR LIBS_INSTALL_PREFIX STREQUAL "")
  if(NOT MYTH_DEFAULT_LIBS_PREFIX STREQUAL "")
    set(LIBS_INSTALL_PREFIX ${MYTH_DEFAULT_LIBS_PREFIX})
  else()
    set(LIBS_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
  endif()
endif()

# Make sure these changes propagate to rebuilds.
set(CMAKE_INSTALL_PREFIX
    ${CMAKE_INSTALL_PREFIX}
    CACHE PATH "" FORCE)
set(LIBS_INSTALL_PREFIX
    ${LIBS_INSTALL_PREFIX}
    CACHE PATH "" FORCE)

#
# Inject code from cmake provided modules. GNUInstallDirs uses
# CMAKE_INSTALL_PREFIX so make sure that's set properly first.
#
include(ExternalProject)
include(FindPkgConfig)
include(GNUInstallDirs)
include(ProcessorCount)

#
# Inject code from mythtv provided modules
#
include(GetLinuxInfo)

#
# Test for required minimum compiler versions.
#
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0.0)
    message(FATAL_ERROR "GCC version 8 or better required.")
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0.0)
    message(FATAL_ERROR "Clang version 11 or better required.")
  endif()
else()
  message(FATAL_ERROR "Unknown compiler.")
endif()

#
# Extract the MythTV source version number (either from Git, or from a file if
# the source has been exported out of git.)
#
include(SourceVersion)
mythtv_find_source_version(MYTHTV_SOURCE_VERSION MYTHTV_SOURCE_VERSION_MAJOR
                           MYTHTV_SOURCE_BRANCH)
message(STATUS "MythTV Source Version: ${MYTHTV_SOURCE_VERSION}")
message(STATUS "MythTV Source Branch: ${MYTHTV_SOURCE_BRANCH}")
if(NOT ${PROJECT_VERSION_MAJOR} EQUAL ${MYTHTV_SOURCE_VERSION_MAJOR})
  message(
    FATAL_ERROR "Major version number mismatch (cmake:${PROJECT_VERSION_MAJOR},"
                " source:.${MYTHTV_SOURCE_VERSION_MAJOR})")
endif()

#
# Is the install directory read-only
#
include(ReadOnlyTests)
ensure_dir_writable("Install" ${CMAKE_INSTALL_PREFIX})
ensure_dir_writable("Install" ${LIBS_INSTALL_PREFIX})

#
# Figure out the CPU count and parallel execution flags.  For a build of any
# given sub-project you can add "-- -j <n>" to the end of the build command to
# perform parallel builds.  This is an attempt to get the superbuild
# orchestration script to properly pass the parallel execution flags to all
# sub-builds.
#
ProcessorCount(CPU_COUNT)
if(NOT CPU_COUNT EQUAL 0)
  message(STATUS "This system has ${CPU_COUNT} CPU(s).")

  if(CMAKE_GENERATOR STREQUAL "Ninja")
    # Try not to kill older Raspberry Pis
    execute_process(
      COMMAND cat /proc/device-tree/model
      OUTPUT_VARIABLE MODEL
      ERROR_QUIET)
    if(MODEL MATCHES "Raspberry Pi (.)")
      if(${CMAKE_MATCH_1} LESS 4)
        set(CPU_COUNT 2)
      endif()
    endif()

    # Building with ninja, 'cmake --build build -j <n>' works properly for cmake
    # based projects, but not for make based projects.  Use this variable to
    # pass the cpu count into make based projects.
    set(MAKE_JFLAG -j${CPU_COUNT})

    # Tell ninja that it can execute as many parallel compile jobs as there are
    # CPUs, but it can only execute one link job at a time. This is probably
    # only necessary for small memory systems, but set it for all systems. Make
    # this a cache variable so that the user can override it from the command
    # line.
    set(CMAKE_JOB_POOLS
        "compile=${CPU_COUNT};link=1"
        CACHE STRING "Job counts for builds using ninja.")

    # Tell ninja that it can execute as many parallel jobs as there are CPUs.
    set(CTEST_BUILD_FLAGS -j${CPU_COUNT})
    set(ctest_test_args ${ctest_test_args} PARALLEL_LEVEL ${CPU_COUNT})
  else()
    # Building with unix makefiles, 'cmake --build build -j <n>' works properly.
  endif()
endif()

#
# Setup default search path for pkg-config files.
#
include(SetSearchPaths)

#
# Perform host and platform specific customizations.  These use variables
# generated by the GNUInstallDirs module, and the SetSearchPath module.
#
include(platform/${CMAKE_SYSTEM_NAME}${_CROSS}Customization OPTIONAL)

include(UpdatePkgConfig)

#
# Find required commands.
#
find_program(MAKE_EXECUTABLE NAMES gmake make REQUIRED)
message(VERBOSE "Found make: ${MAKE_EXECUTABLE}")
if(CMAKE_GENERATOR STREQUAL "Ninja")
  find_program(NINJA_EXECUTABLE NAMES ninja REQUIRED)
  message(VERBOSE "Found ninja: ${NINJA_EXECUTABLE}")
endif()
find_program(PATCH_EXECUTABLE NAMES patch REQUIRED)
message(VERBOSE "Found patch: ${PATCH_EXECUTABLE}")
if(CMAKE_CROSSCOMPILING)
  find_program(MESON_EXECUTABLE NAMES meson REQUIRED)
  message(VERBOSE "Found meson: ${MESON_EXECUTABLE}")
  find_program(MESON_EXECUTABLE NAMES gperf REQUIRED)
  message(VERBOSE "Found gperf: ${GPERF_EXECUTABLE}")
endif()

#
# Check compiler and installed header files
#
include(CompilerCaching)
include(SetCompilerOptions)

#
# Build a set of *.pc files for this platform.
#
include(platform/${CMAKE_SYSTEM_NAME}${_CROSS}BuildPkgConfigFiles OPTIONAL)

#
# For the next set of includes, update the CMDLINE_ARGS variable to change the
# install prefix to LIBS.  This will make the external cmake based projects
# install into LIBS.  The original value will need to be restored after the libs
# are built.
#
list(REMOVE_DUPLICATES CMDLINE_ARGS)
list(REMOVE_DUPLICATES PLATFORM_ARGS)
set(CMDLINE_ARGS_LIBS ${CMDLINE_ARGS})
list(APPEND CMDLINE_ARGS_LIBS
     "-DCMAKE_INSTALL_PREFIX:PATH=${LIBS_INSTALL_PREFIX}")
list(APPEND CMDLINE_ARGS "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}"
     "-DLIBS_INSTALL_PREFIX:PATH=${LIBS_INSTALL_PREFIX}")

#
# Create targets for building subsets of the needed libraries.
#
add_custom_target(external_libs)
add_custom_target(embedded_libs)
if(ENABLE_STRICT_BUILD_ORDER)
  add_dependencies(embedded_libs external_libs)
endif()
add_custom_target(all_libs DEPENDS external_libs embedded_libs)
add_custom_target(libs DEPENDS all_libs)

#
# Find (or build) system libraries
#
if(CMAKE_CROSSCOMPILING)
  message(VERBOSE "Setting up sub-projects for the external dependencies.")
  # Build the dependencies that will be needed later to build MythTV.
  include(BuildExternalLibraries)
else()
  # Validate the dependencies that will be needed later to build MythTV itself.
  # Adding checks here causes cmake to "fail early" while running cmake on the
  # superbuild, instead of failing much later while running make/ninja to build
  # mythtv.
  include(CheckExternalLibraries)
endif()

#
# Debugging code
#
# Add "--log-level=VERBOSE" to the cmake command to generate this output.
#
message(VERBOSE "find_xxx related variables:")
list(APPEND CMAKE_MESSAGE_INDENT "  ")
message(VERBOSE "CMAKE_FIND_ROOT_PATH: ${CMAKE_FIND_ROOT_PATH}")
message(
  VERBOSE
  "CMAKE_FIND_ROOT_PATH_MODE_INCLUDE: ${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}")
message(
  VERBOSE
  "CMAKE_FIND_ROOT_PATH_MODE_LIBRARY: ${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}")
message(
  VERBOSE
  "CMAKE_FIND_ROOT_PATH_MODE_PACKAGE: ${CMAKE_FIND_ROOT_PATH_MODE_PACKAGE}")
message(
  VERBOSE
  "CMAKE_FIND_ROOT_PATH_MODE_PROGRAM: ${CMAKE_FIND_ROOT_PATH_MODE_PROGRAM}")
message(
  VERBOSE
  "CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH: ${CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH}"
)
message(VERBOSE
        "CMAKE_FIND_USE_CMAKE_SYSTEM_PATH: ${CMAKE_FIND_USE_CMAKE_SYSTEM_PATH}")
message(
  VERBOSE
  "CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH: ${CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH}"
)
message(VERBOSE "CMAKE_FRAMEWORK_PATH: ${CMAKE_FRAMEWORK_PATH}")
message(VERBOSE "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message(VERBOSE "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(VERBOSE "CMAKE_PROGRAM_PATH: ${CMAKE_PROGRAM_PATH}")
list(POP_BACK CMAKE_MESSAGE_INDENT)
message(VERBOSE "pkg-config related variables:")
list(APPEND CMAKE_MESSAGE_INDENT "  ")
message(VERBOSE "PKG_CONFIG_COMMAND : ${PKG_CONFIG_COMMAND}")
message(VERBOSE "PKG_CONFIG_ARGN : ${PKG_CONFIG_ARGN}")
message(VERBOSE "PKG_CONFIG_LIBDIR : ${PKG_CONFIG_LIBDIR}")
message(VERBOSE "PKG_CONFIG_PATH : ${PKG_CONFIG_PATH}")
list(POP_BACK CMAKE_MESSAGE_INDENT)

if(DUMP_VARIABLES)
  include(DumpAllVariables)
  dump_cmake_variables()
  message(FATAL_ERROR "Variables dumped.")
endif()

# ---------------------------------------------------

#
# Everything below here is setting up the sub-projects to be built.
#

# ---------------------------------------------------

#
# Add the build instructions for libraries that are embedded as part of the
# mythtv sources.
#
message(VERBOSE "Setting up sub-projects for embedded libraries.")
include(embeddedlibs/FindOrBuildNvCodecHeaders)
include(embeddedlibs/FindOrBuildUdfRead)
include(externallibs/FindOrBuildExiv2)
include(embeddedlibs/FindOrBuildFFmpeg)

#
# Always build MythTV
#
message(VERBOSE "Setting up a sub-project for the mythtv directory.")
ExternalProject_Add(
  MythTV
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mythtv
  CMAKE_ARGS ${CMDLINE_ARGS}
             ${PLATFORM_ARGS}
             ${USES_QT_PLATFORM_ARGS}
             -DSUPER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}
             -DSUPER_VERSION:STRING=${PROJECT_VERSION}
             -DMYTHTV_SOURCE_VERSION:STRING=${MYTHTV_SOURCE_VERSION}
             -DMYTHTV_SOURCE_BRANCH:STRING=${MYTHTV_SOURCE_BRANCH}
  CMAKE_CACHE_ARGS
    -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
    -DCMAKE_JOB_POOL_COMPILE:STRING=compile
    -DCMAKE_JOB_POOL_LINK:STRING=link
    -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
    -DPKG_CONFIG_LIBDIR:PATH=${PKG_CONFIG_LIBDIR}
    -DPKG_CONFIG_PATH:PATH=${PKG_CONFIG_PATH}
  BUILD_ALWAYS TRUE
  USES_TERMINAL_CONFIGURE TRUE
  USES_TERMINAL_BUILD TRUE
  USES_TERMINAL_TEST TRUE
  DEPENDS all_libs)

include(MiscellaneousTargets)

#
# Maybe build MythTV plugins
#
if(MYTH_BUILD_PLUGINS)
  message(VERBOSE "Setting up a sub-project for the mythplugins directory.")
  ExternalProject_Add(
    MythPlugins
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mythplugins
    CMAKE_ARGS ${CMDLINE_ARGS} ${PLATFORM_ARGS} ${USES_QT_PLATFORM_ARGS}
               -DSUPER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}
               -DSUPER_VERSION:STRING=${PROJECT_VERSION}
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:PATH=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:PATH=${PKG_CONFIG_PATH}
    BUILD_ALWAYS TRUE
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    DEPENDS MythTV)
else()
  add_custom_target(MythPlugins) # Dummy
  message(VERBOSE
          "Skipping setting up a sub-project for the mythplugins directory.")
endif()

#
# Maybe build translations executable
#
if(MYTH_BUILD_THEMESTRING_TOOL AND NOT CMAKE_CROSSCOMPILING)
  message(VERBOSE "Setting up a sub-project for the themestringtool directory.")
  ExternalProject_Add(
    ThemeStringTool
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/themestringstool
    CMAKE_ARGS ${CMDLINE_ARGS}
               ${PLATFORM_ARGS}
               ${USES_QT_PLATFORM_ARGS}
               -DQT_VERSION_MAJOR:STRING=${QT_VERSION_MAJOR}
               -DSUPER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}
               -DSUPER_VERSION:STRING=${PROJECT_VERSION}
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:PATH=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:PATH=${PKG_CONFIG_PATH}
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "Nothing to install."
    USES_TERMINAL_BUILD TRUE
    DEPENDS MythTV MythPlugins
    EXCLUDE_FROM_ALL FALSE)
else()
  message(
    VERBOSE
    "Skipping setting up a sub-project for the themestringtool directory.")
endif()

# Package the results
include(platform/${CMAKE_SYSTEM_NAME}${_CROSS}Packaging OPTIONAL)

include(StaticAnalysis)
