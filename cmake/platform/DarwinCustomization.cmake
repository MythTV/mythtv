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
  RESULT_VARIABLE DETECT_MACPORTS
  OUTPUT_VARIABLE MACPORTS_PREFIX
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
  COMMAND brew --prefix
  RESULT_VARIABLE DETECT_HOMEBREW
  OUTPUT_VARIABLE HOMEBREW_PREFIX
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(DETECT_MACPORTS EQUAL 0)
  # The MACPORTS_PREFIX variable should contain a path name like
  # "/opt/local/bin/port". Go up two levels to find the "root" directory for
  # macports.
  string(REPLACE "/bin/port" "" MACPORTS_PREFIX ${MACPORTS_PREFIX})

  # Add macports specific include and lib directories.
  link_directories(AFTER "${MACPORTS_PREFIX}/lib")
  include_directories(AFTER SYSTEM "${MACPORTS_PREFIX}/include")

  # FFmpeg needs a little help in finding the mp3lame library.
  list(APPEND FF_PLATFORM_ARGS "--extra-ldflags=-L${MACPORTS_PREFIX}/lib"
       "--extra-cflags=-I${MACPORTS_PREFIX}/include")

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
elseif(DETECT_HOMEBREW EQUAL 0)
  # Add homebrew specific include and lib directories.
  link_directories(AFTER "${HOMEBREW_PREFIX}/lib")
  include_directories(AFTER SYSTEM "${HOMEBREW_PREFIX}/include")

  # FFmpeg needs a little help in finding the mp3lame library.
  list(APPEND FF_PLATFORM_ARGS "--extra-ldflags=-L${HOMEBREW_PREFIX}/lib"
       "--extra-cflags=-I${HOMEBREW_PREFIX}/include")

  # Qt6 builds need a little help finding the libraries.
  set(_QT_BASE "${HOMEBREW_PREFIX}/opt/${QT_PKG_NAME_LC}")

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
endif(DETECT_MACPORTS EQUAL 0)

# Qt6 builds need a little help finding the libraries.
list(APPEND CMAKE_FRAMEWORK_PATH "${_QT_BASE}")
list(APPEND CMAKE_MODULE_PATH "${_QT_BASE}/lib/cmake")

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
if("${CMAKE_C_COMPILER}" MATCHES "Xcode")
  if(${CMAKE_C_COMPILER_VERSION} VERSION_GREATER_EQUAL 13)
    message(STATUS "Adding Apple Clang '-lSystem' hack")
    list(
      APPEND
      FF_PLATFORM_ARGS
      "--extra-ldflags=-L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib"
    )
    list(
      APPEND
      FF_PLATFORM_ARGS
      "--extra-cflags=-I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include"
    )
  endif()
endif()
