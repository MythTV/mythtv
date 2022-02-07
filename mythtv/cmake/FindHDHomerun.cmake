# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindHDHomeRun
--------

Find the HDHomeRun library (``hdhomerun``, https://www.silicondust.com/).

Imported targets
^^^^^^^^^^^^^^^^

.. versionadded:: MythTV 33.0

This module defines the following :prop_tgt:`IMPORTED` targets:

``HDHomerun::HDHomerun``
  The HDHomerun::HDHomerun library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``HDHomerun_FOUND``
  true if the HDHomerun headers and libraries were found
``HDHomerun_INCLUDE_DIR``
  the directory containing the HDHomerun headers
``HDHomerun_INCLUDE_DIRS``
  the directory containing the HDHomerun headers
``HDHomerun_LIBRARIES``
  HDHomerun libraries to be linked

#]=======================================================================]

include(CMakePushCheckState)
include(CheckSymbolExists)
include(CheckLibraryExists)

find_file(
  HDHomerun_H_FILE
  NAMES "hdhomerun.h"
  PATH_SUFFIXES "hdhomerun" "libhdhomerun")
cmake_path(GET HDHomerun_H_FILE PARENT_PATH HDHomerun_INCLUDE_DIR)

find_library(HDHomerun_LIBRARY NAMES hdhomerun)

if(HDHomerun_INCLUDE_DIR AND HDHomerun_LIBRARY)
  set(HDHomerun_FOUND 1)
endif()

#
# HDHomerun doesn't supply a version number. Look for the exitence of various
# functions in the header files.
#
if(HDHomerun_FOUND)
  cmake_path(GET HDHomerun_LIBRARY PARENT_PATH LIB_DIR)
  check_library_exists(hdhomerun hdhomerun_discover_find_devices LIB_DIR
                       HDHOMERUN_V1)
  check_library_exists(hdhomerun hdhomerun_discover_find_devices_v2 LIB_DIR
                       HDHOMERUN_V2)
  check_library_exists(hdhomerun hdhomerun_discover_find_devices_v3 LIB_DIR
                       HDHOMERUN_V3)
  check_library_exists(hdhomerun hdhomerun_device_selector_load_from_str
                       LIB_DIR HDHOMERUN_DEVICE_SELECTOR_LOAD_FROM_STR)
endif()

#
# Do standard 'Find' stuff from here onwards.
#
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  HDHomerun
  FOUND_VAR HDHomerun_FOUND
  REQUIRED_VARS HDHomerun_LIBRARY HDHomerun_INCLUDE_DIR)

# Traditional variables
if(HDHomerun_FOUND)
  set(HDHomerun_LIBRARIES ${HDHomerun_LIBRARY})
  set(HDHomerun_INCLUDE_DIRS ${HDHomerun_INCLUDE_DIR})
endif()

# Imported target
if(HDHomerun_FOUND AND NOT TARGET HDHomerun::HDHomerun)
  add_library(HDHomerun::HDHomerun UNKNOWN IMPORTED)
  set_target_properties(
    HDHomerun::HDHomerun
    PROPERTIES
      IMPORTED_LOCATION "${HDHomerun_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${HDHomerun_INCLUDE_DIR}"
      INTERFACE_COMPILE_DEFINITIONS
      "USING_HDHOMERUN;HDHOMERUN_HEADERFILE=\"${HDHomerun_H_FILE}\";$<$<BOOL:HDHOMERUN_V1>:HDHOMERUN_V1>;$<$<BOOL:HDHOMERUN_V2>:HDHOMERUN_V2>;$<$<BOOL:HDHOMERUN_V3>:HDHOMERUN_V3>;$<$<BOOL:HDHOMERUN_DEVICE_SELECTOR_LOAD_FROM_STR>:HDHOMERUN_DEVICE_SELECTOR_LOAD_FROM_STR>"
  )
endif()

mark_as_advanced(
  HDHomerun_INCLUDE_DIR HDHomerun_LIBRARY HDHOMERUN_V1 HDHOMERUN_V2
  HDHOMERUN_V3 HDHOMERUN_DEVICE_SELECTOR_LOAD_FROM_STR)
