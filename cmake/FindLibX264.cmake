# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindX264
--------

Find the X264 library (``libx264``, https://www.videolan.org/developers/x264.html).

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``X264::X264``
  The X264 library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``X264_FOUND``
  true if the X264 headers and libraries were found
``X264_INCLUDE_DIR``
  the directory containing the X264 headers
``X264_INCLUDE_DIRS``
  the directory containing the X264 headers
``X264_LIBRARIES``
  X264 libraries to be linked

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``X264_INCLUDE_DIR``
  the directory containing the X264 headers

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_LibX264 QUIET x264)
if(NOT PC_LibX264_FOUND)
  return()
endif()

find_path(
  LibX264_INCLUDE_DIR
  NAMES x264.h
  PATHS ${PC_LibX264_INCLUDE_DIRS})
find_library(
  LibX264_LIBRARY
  NAMES x264
  PATHS ${PC_LibX264_LIBRARY_DIRS})
set(LibX264_VERSION ${PC_LibX264_VERSION})

set(_symbolRegex "^#define +X264_BUILD +([^ ]+) *$")
file(STRINGS "${LibX264_INCLUDE_DIR}/x264.h" _contents REGEX "${_symbolRegex}")
if(_contents MATCHES "([0-9]+)")
  set(LibX264_BUILD ${CMAKE_MATCH_1})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LibX264
  REQUIRED_VARS LibX264_LIBRARY LibX264_INCLUDE_DIR LibX264_BUILD
  VERSION_VAR LibX264_VERSION)

# Traditional variables
if(LibX264_FOUND)
  set(LibX264_LIBRARIES ${LibX264_LIBRARY})
  set(LibX264_INCLUDE_DIRS ${LibX264_INCLUDE_DIR})
  set(LibX264_DEFINITIONS ${PC_LibX264_CFLAGS_OTHER})
endif()

# Imported target
if(LibX264_FOUND AND NOT TARGET LibX264::LibX264)
  add_library(LibX264::LibX264 UNKNOWN IMPORTED)
  set_target_properties(
    LibX264::LibX264
    PROPERTIES IMPORTED_LOCATION "${LibX264_LIBRARY}"
               INTERFACE_COMPILE_OPTIONS "${PC_LibX264_CFLAGS_OTHER}"
               INTERFACE_INCLUDE_DIRECTORIES "${LibX264_INCLUDE_DIR}"
               INTERFACE_LINK_LIBRARIES "Threads::Threads m ${CMAKE_DL_LIBS}")
endif()

mark_as_advanced(LibX264_INCLUDE_DIR LibX264_LIBRARY)
