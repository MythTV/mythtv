# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLame
--------

Find the MP3Lame library (``libmp3lame``, https://lame.sourceforge.io/).

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``Lame::Lame``
  The Lame library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Lame_FOUND``
  true if the Lame headers and libraries were found
``Lame_INCLUDE_DIR``
  the directory containing the Lame headers
``Lame_LIBRARIES``
  Lame libraries to be linked
``Lame_VERSION``
  The version of the lame library found.  This will be "unknown" if
  cross compiling.

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Lame_INCLUDE_DIR``
  the directory containing the Lame headers

#]=======================================================================]

include(CMakePushCheckState)
include(CheckCSourceRuns)

find_package(PkgConfig)

#
# Find installed mp3lame files
#
find_path(Lame_INCLUDE_DIR NAMES lame/lame.h)
if(NOT Lame_INCLUDE_DIR)
  message(VERBOSE "mp3lame: Couldn't find header file.")
  return()
endif()

find_library(Lame_LIBRARY NAMES mp3lame)
if(NOT Lame_LIBRARY)
  message(VERBOSE "mp3lame: Couldn't find library.")
  return()
endif()

#
# The lame version number isn't available in the header file. You have to call
# the lame API to get the version string. Build an executable file to do just
# that. This will only work for native compiles.
#
file(
  WRITE ${PROJECT_BINARY_DIR}/tmp/lame_print_version.c
  "
  #include <lame/lame.h>
  int main (int argc, char **argv)
  {
    printf(\"%s\", get_lame_short_version());
  }
  ")
if(NOT CMAKE_CROSSCOMPILING)
  try_compile(
    _compiled ${PROJECT_BINARY_DIR}/tmp
    ${PROJECT_BINARY_DIR}/tmp/lame_print_version.c
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:PATH=${Lame_INCLUDE_DIR}"
    LINK_LIBRARIES ${Lame_LIBRARY}
    COPY_FILE ${PROJECT_BINARY_DIR}/tmp/lame_print_version)
  if(_compiled)
    execute_process(COMMAND ${PROJECT_BINARY_DIR}/tmp/lame_print_version
                    OUTPUT_VARIABLE Lame_VERSION)
  endif()
else()
  set(Lame_VERSION "unknown")
endif()

#
# Now do the standard packaging stuff.
#
include(FindPackageHandleStandardArgs)
if(NOT Lame_VERSION STREQUAL "unknown")
  find_package_handle_standard_args(
    Lame
    REQUIRED_VARS Lame_INCLUDE_DIR Lame_LIBRARY Lame_VERSION
    VERSION_VAR Lame_VERSION)
else()
  find_package_handle_standard_args(Lame REQUIRED_VARS Lame_INCLUDE_DIR
                                                       Lame_LIBRARY)
endif()

# Traditional variables
if(Lame_FOUND)
  set(Lame_LIBRARIES ${Lame_LIBRARY})
  set(Lame_INCLUDE_DIRS ${Lame_INCLUDE_DIR})
endif()

# Imported target
if(Lame_FOUND AND NOT TARGET Lame::Lame)
  add_library(Lame::Lame UNKNOWN IMPORTED)
  set_target_properties(
    Lame::Lame PROPERTIES IMPORTED_LOCATION "${Lame_LIBRARY}"
                          INTERFACE_INCLUDE_DIRECTORIES "${Lame_INCLUDE_DIR}")
endif()

mark_as_advanced(Lame_INCLUDE_DIR Lame_LIBRARY)
