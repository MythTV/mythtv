# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindXvid
--------

Find the Xvid library (``libxvidcore``, https://www.xvid.com/).

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``Xvid::Xvid``
  The Xvid library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Xvid_FOUND``
  true if the Xvid headers and libraries were found
``Xvid_INCLUDE_DIR``
  the directory containing the Xvid headers
``Xvid_INCLUDE_DIRS``
  the directory containing the Xvid headers ``Xvid_LIBRARIES``
  Xvid libraries to be linked

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Xvid_INCLUDE_DIR``
  the directory containing the Xvid headers

#]=======================================================================]

function(parse_xvid_version)
  set(_versionRegex "^#define +XVID_VERSION +XVID_MAKE_VERSION\\((.*)\\) *$")
  file(STRINGS "${_XVID_H_PATH}" _contents REGEX "${_versionRegex}")
  if(_contents MATCHES "([0-9]+),([0-9]+),([0-9]+)")
    set(LibXvid_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(LibXvid_VERSION_MINOR ${CMAKE_MATCH_2})
    set(LibXvid_VERSION_PATCH ${CMAKE_MATCH_3})
    set(LibXvid_VERSION
        "${LibXvid_VERSION_MAJOR}.${LibXvid_VERSION_MINOR}.${LibXvid_VERSION_PATCH}"
        PARENT_SCOPE)
  endif()
endfunction()

function(parse_xvid_api_version)
  set(_apiRegex "^#define +XVID_API +XVID_MAKE_API\\((.*)\\) *$")
  file(STRINGS "${_XVID_H_PATH}" _contents REGEX "${_apiRegex}")
  if(_contents MATCHES "([0-9]+), *([0-9]+)")
    set(LibXvid_API_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(LibXvid_API_VERSION_MINOR ${CMAKE_MATCH_2})
    set(LibXvid_API_VERSION
        "${LibXvid_API_VERSION_MAJOR}.${LibXvid_API_VERSION_MINOR}"
        PARENT_SCOPE)
  endif()
endfunction()

function(parse_xvid_bs_version)
  set(_symbolRegex "^#define *XVID_BS_VERSION *([0-9]+) *$")
  file(STRINGS ${_XVID_H_PATH} _contents REGEX "${_symbolRegex}")
  if(_contents MATCHES "([0-9]+)")
    set(LibXvid_BS_VERSION
        ${CMAKE_MATCH_1}
        PARENT_SCOPE)
  endif()
endfunction()

find_path(LibXvid_INCLUDE_DIR NAMES xvid.h)
if(NOT LibXvid_INCLUDE_DIR)
  return()
endif()
set(_XVID_H_PATH "${LibXvid_INCLUDE_DIR}/xvid.h")
find_library(LibXvid_LIBRARY NAMES xvidcore)
parse_xvid_version()
parse_xvid_api_version()
parse_xvid_bs_version()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LibXvid
  REQUIRED_VARS LibXvid_LIBRARY LibXvid_INCLUDE_DIR LibXvid_API_VERSION
                LibXvid_BS_VERSION
  VERSION_VAR LibXvid_VERSION)

# Traditional variables
if(LibXvid_FOUND)
  set(LibXvid_LIBRARIES ${LibXvid_LIBRARY})
  set(LibXvid_INCLUDE_DIRS ${LibXvid_INCLUDE_DIR})
endif()

# Imported target
if(LibXvid_FOUND AND NOT TARGET LibXvid::LibXvid)
  add_library(LibXvid::LibXvid UNKNOWN IMPORTED)
  set_target_properties(
    LibXvid::LibXvid
    PROPERTIES IMPORTED_LOCATION "${LibXvid_LIBRARY}"
               INTERFACE_INCLUDE_DIRECTORIES "${LibXvid_INCLUDE_DIR}")
endif()

mark_as_advanced(LibXvid_INCLUDE_DIR LibXvid_LIBRARY)
