#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT APPLE)
  return()
endif()

#
# Apple builds require Objective C++ compiler.
#
include(CheckLanguage)
check_language(OBJCXX)

# MacPorts or Homebrew?
execute_process(
  COMMAND which port
  RESULT_VARIABLE _DETECT_MACPORTS
  OUTPUT_VARIABLE MACPORTS_PREFIX
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
  COMMAND brew --prefix
  RESULT_VARIABLE _DETECT_HOMEBREW
  OUTPUT_VARIABLE HOMEBREW_PREFIX
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(_DETECT_MACPORTS EQUAL 0)
  # The MACPORTS_PREFIX variable should contain a path name like
  # "/opt/local/bin/port". Go up two levels to find the "root" directory for
  # macports.
  string(REPLACE "/bin/port" "" MACPORTS_PREFIX ${MACPORTS_PREFIX})

  # Add macports specific include and lib directories.
  link_directories(AFTER "${MACPORTS_PREFIX}/lib")
  include_directories(AFTER SYSTEM "${MACPORTS_PREFIX}/include")

  # FFmpeg needs a little help in finding the mp3lame library.
  list(APPEND FF_C_CXX_FLAGS "-I${MACPORTS_PREFIX}/include")
  list(APPEND FF_LD_FLAGS "-L${MACPORTS_PREFIX}/lib")

  # Qt6 builds need a little help finding the libraries.
  set(_QT_BASE "${MACPORTS_PREFIX}/libexec/${QT_PKG_NAME_LC}")

  # Provide the default MacPorts location for Python3, if the the user hasn't
  # already specified a value in their options override file. This prevents
  # finding the Apple installed version.
  if(Python3_ROOT_DIR STREQUAL "")
    set(Python3_ROOT_DIR
        "${MACPORTS_PREFIX}/Library/Frameworks/Python.framework/Versions/Current"
    )
  endif()

  # Informational in case needed elsewhere.
  message(STATUS "Detected MacPorts (${MACPORTS_PREFIX})")
  set(MACPORTS ON)
elseif(_DETECT_HOMEBREW EQUAL 0)
  # Add homebrew specific include and lib directories.
  link_directories(AFTER "${HOMEBREW_PREFIX}/lib")
  include_directories(AFTER SYSTEM "${HOMEBREW_PREFIX}/include")

  # FFmpeg needs a little help in finding the mp3lame library.
  list(APPEND FF_C_CXX_FLAGS "-I${HOMEBREW_PREFIX}/include")
  list(APPEND FF_LD_FLAGS "-L${HOMEBREW_PREFIX}/lib")

  # Homebrew needs a little help finding the QT libraries due to the the HB
  # naming convention which uses an @ symbol
  set(QT_PKG_NAME_HB "qt@${QT_VERSION_MAJOR}")
  set(_QT_BASE "${HOMEBREW_PREFIX}/opt/${QT_PKG_NAME_HB}")

  # Provide the default Homebrew location for Python3, if the the user hasn't
  # already specified a value in their options override file. This prevents
  # finding the Apple installed version.
  if(Python3_ROOT_DIR STREQUAL "")
    set(Python3_ROOT_DIR
        "${HOMEBREW_PREFIX}/Frameworks/Python.framework/Versions/Current")
  endif()

  # A couple more libraries that homebrew puts into their own subdirectories.
  # These are added as cache variables so that a developer can override them on
  # the command line or in an options override file.
  set(SQLite3_ROOT
      "${HOMEBREW_PREFIX}/opt/sqlite/"
      CACHE STRING "The directory where SQLite3 is installed.")
  set(ZLIB_ROOT
      "${HOMEBREW_PREFIX}/opt/zlib/"
      CACHE STRING "The directory where zlib is installed.")
  set(Iconv_ROOT
      "${HOMEBREW_PREFIX}/opt/libiconv/"
      CACHE STRING "The directory where iconv is installed.")

  # Informational in case needed elsewhere.
  message(STATUS "Detected Homebrew (${HOMEBREW_PREFIX})")
  set(HOMEBREW ON)
else()
  message(FATAL_ERROR "Not building with MacPorts or Homebrew.")
endif(_DETECT_MACPORTS EQUAL 0)

# Check to see if a python virtual environment is set, and if so
# tell cmake to use those site-packages instead of the default
#  Macports or Homebrew python versions.
if(DEFINED ENV{VIRTUAL_ENV})
  #set (CMAKE_FIND_FRAMEWORK NEVER)
  set (Python3_FIND_VIRTUALENV ONLY)
  set(Python3_ROOT_DIR $ENV{VIRTUAL_ENV})
endif()

# Qt6 builds need a little help finding the libraries.
list(APPEND CMAKE_FRAMEWORK_PATH "${_QT_BASE}")
list(APPEND CMAKE_MODULE_PATH "${_QT_BASE}/lib/cmake")
# Qt6 builds need a little help finding the plugins.
set(ENV{QT_PLUGIN_PATH} "${_QT_BASE}/share/qt/plugins")
set(ENV{QT_QPA_PLATFORM_PLUGIN_PATH} "${_QT_BASE}/share/qt/plugins")

# ~~~
# Using the Apple Clang compiler (version 13 tested) spits out this error:
#
#    ld: library not found for -lSystem
#
# According to the internet, the fix is to add the extra -L argument
# below.  That, however, produces a different set of warnings about
# missing include files.  The include file warnings are solved by the
# extra -I argument below.
# ~~~
#
# depending on if commandline tools are installed, the developer root
# may be /Applications/Xcode or /Library/Developer/CommandLineTools
# check for a compiler match in either, then use CMAKE_OSX_SYSROOT to path the
# correct system indlude, sysroot, syslibroot, and library search paths
if("${CMAKE_C_COMPILER}" MATCHES "Xcode" OR "${CMAKE_C_COMPILER}" MATCHES "CommandLineTools")
  if(${CMAKE_C_COMPILER_VERSION} VERSION_GREATER_EQUAL 13)
    # depending on if commandline tools are installed, the developer root
    # may be /Applications/Xcode or /Library/Developer/CommandLineTools
    message(STATUS "Adding Apple Clang '-lSystem' hack")

    list(APPEND FF_C_CXX_FLAGS "-I${CMAKE_OSX_SYSROOT}/usr/include"
                               "-isysroot${CMAKE_OSX_SYSROOT}")

    list(APPEND FF_LD_FLAGS    "-L${CMAKE_OSX_SYSROOT}/usr/lib"
                               "-Wl,-syslibroot,${CMAKE_OSX_SYSROOT}")
  endif()
endif()

# Loop over the added C and CXX flags adding flags for both
# extra and host for c and cxx/cpp inputs
foreach(FLAG IN LISTS FF_C_CXX_FLAGS)
  list(APPEND FF_PLATFORM_ARGS "--extra-cflags=${FLAG}"
                               "--host-cflags=${FLAG}"
                               "--extra-cxxflags=${FLAG}"
                               "--host-cppflags=${FLAG}")
endforeach()

# Loop over the added LDFLAGS adding flags for both extra and host
foreach(FLAG IN LISTS FF_LD_FLAGS)
  list(APPEND FF_PLATFORM_ARGS "--extra-ldflags=${FLAG}"
                               "--host-ldflags=${FLAG}")
endforeach()

# if we're signing an application, bundling must be enabled
if (DARWIN_GENERATE_DISTRIBUTION AND NOT DARWIN_FRONTEND_BUNDLE)
  message(FATAL_ERROR "Error: Generating a Drag And Drop Installer requires at least one App Bundle to be made.")
endif()

# Miscellaneous defines
set(CONFIG_APPLEREMOTE TRUE)
