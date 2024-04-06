# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindX265
--------

Find the X265 library (``libtiff``, http://x265.org/).

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``X265::X265``
  The X265 library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``X265_FOUND``
  true if the X265 headers and libraries were found
``X265_INCLUDE_DIR``
  the directory containing the X265 headers
``X265_INCLUDE_DIRS``
  the directory containing the X265 headers ``X265_LIBRARIES``
  X265 libraries to be linked

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``X265_INCLUDE_DIR``
  the directory containing the X265 headers

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_LibX265 QUIET x265)
if(NOT PC_LibX265_FOUND)
  return()
endif()

find_path(
  LibX265_INCLUDE_DIR
  NAMES x265.h
  PATHS ${PC_LibX265_INCLUDE_DIRS})
find_library(
  LibX265_LIBRARY
  NAMES x265
  PATHS ${PC_LibX265_LIBRARY_DIRS})
set(LibX265_VERSION ${PC_LibX265_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LibX265
  REQUIRED_VARS LibX265_LIBRARY LibX265_INCLUDE_DIR
  VERSION_VAR LibX265_VERSION)

# Traditional variables
if(LibX265_FOUND)
  set(LibX265_LIBRARIES ${LibX265_LIBRARY})
  set(LibX265_INCLUDE_DIRS ${LibX265_INCLUDE_DIR})
  set(LibX265_DEFINITIONS ${PC_LibX265_CFLAGS_OTHER})
endif()

# Imported target
if(LibX265_FOUND AND NOT TARGET LibX265::LibX265)
  add_library(LibX265::LibX265 UNKNOWN IMPORTED)
  set_target_properties(
    LibX265::LibX265
    PROPERTIES IMPORTED_LOCATION "${LibX265_LIBRARY}"
               INTERFACE_COMPILE_OPTIONS "${PC_LibX265_CFLAGS_OTHER}"
               INTERFACE_INCLUDE_DIRECTORIES "${LibX265_INCLUDE_DIR}"
               # INTERFACE_LINK_LIBRARIES "Threads::Threads m
               # ${CMAKE_DL_LIBS}"
  )
endif()

mark_as_advanced(LibX265_INCLUDE_DIR LibX265_LIBRARY)
