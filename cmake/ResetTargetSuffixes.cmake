#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# Qt on android configures cmake to include the target build architecture as
# part of the name of all the shared objects.  Reset these values to the
# suffixes used by MythTV.
macro(reset_target_suffixes ADD_VERSION)
  if(${ADD_VERSION})
    # Re-configure cmake to the MythTV style of embedding the version number.
    set(CMAKE_SHARED_LIBRARY_SUFFIX_C
        "-${CMAKE_PROJECT_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(CMAKE_SHARED_LIBRARY_SUFFIX_CXX
        "-${CMAKE_PROJECT_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(CMAKE_SHARED_MODULE_SUFFIX_C
        "-${CMAKE_PROJECT_VERSION_MAJOR}${CMAKE_SHARED_MODULE_SUFFIX}")
    set(CMAKE_SHARED_MODULE_SUFFIX_CXX
        "-${CMAKE_PROJECT_VERSION_MAJOR}${CMAKE_SHARED_MODULE_SUFFIX}")
    if(CMAKE_IMPORT_LIBRARY_SUFFIX)
      set(CMAKE_IMPORT_LIBRARY_SUFFIX_C
          "-${CMAKE_PROJECT_VERSION_MAJOR}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
      set(CMAKE_IMPORT_LIBRARY_SUFFIX_CXX
          "-${CMAKE_PROJECT_VERSION_MAJOR}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
    endif()
  else()
    unset(CMAKE_SHARED_LIBRARY_SUFFIX_C)
    unset(CMAKE_SHARED_LIBRARY_SUFFIX_CXX)
    unset(CMAKE_SHARED_MODULE_SUFFIX_C)
    unset(CMAKE_SHARED_MODULE_SUFFIX_CXX)
  endif()
endmacro()
