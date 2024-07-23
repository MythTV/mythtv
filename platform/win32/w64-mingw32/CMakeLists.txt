#
# Copyright (C) 2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# Can use cmake features introduced in 3.20.
cmake_minimum_required(VERSION 3.20)

#
# Validate parameters
#
foreach(_param IN ITEMS SUPER_VERSION MYTHTV_SOURCE_VERSION)
  if(NOT ${_param})
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
# From from earlier builds in the superbuild:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/cmake")

#
# Describe this project
#
project(
  WindowsPackaging
  VERSION ${SUPER_VERSION}
  LANGUAGES CXX)
include(VersionInformation)

# Save variables for rebuilds
if(NOT TOOLCHAIN_PREFIX)
  message(FATAL_ERROR "TOOLCHAIN_PREFIX must be set.")
endif()
set(TOOLCHAIN_PREFIX
    ${TOOLCHAIN_PREFIX}
    CACHE STRING "")

#
# Inject code from cmake provided modules. GNUInstallDirs uses
# CMAKE_INSTALL_PREFIX so make sure that's set properly first.
#
include(GNUInstallDirs)
include(DumpAllVariables)

#
# Require the C++17 standard, and allow compiler extensions.
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Inject code from super-project provided modules (part 1).  We need the options
# before setting up variables for Qt.
include(MythOptions)
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES1}")
include(${MYTH_USER_OVERRIDES1} OPTIONAL)
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES2}")
include(${MYTH_USER_OVERRIDES2} OPTIONAL)

include(SetSearchPaths)

if(MYTH_VERSIONED_EXTENSIONS)
  set(MYTH_SUFFIX "-${CMAKE_PROJECT_VERSION_MAJOR}")
endif()

#
# Create top directory to be zipped
#
file(MAKE_DIRECTORY install/bin/plugin)

#
# Given a directory and a target pattern, for each matching file do the
# following:
#
# 1) symlink the file into the specified directory
#
# 2) create a dependency upon the original file
#
function(link_files TYPE BASE PATTERN TARGET)
  # message(DEBUG "link_files(${TYPE} ${BASE} ${PATTERN} ${TARGET})")
  file(${TYPE} FILES RELATIVE ${BASE} ${BASE}/${PATTERN})
  # message(DEBUG "FILES: ${FILES}")
  foreach(FILE IN LISTS FILES)
    file(CREATE_LINK ${BASE}/${FILE} ${TARGET}/${FILE} COPY_ON_ERROR SYMBOLIC)
    list(APPEND ALLFILES ${BASE}/${FILE})
  endforeach()
endfunction()

#
# Consolidate all files in the LIBS_INSTALL_PREFIX and CMAKE_INSTALL_PREFIX
# directories into a single location.
#
set(PREFIXES ${LIBS_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX})
list(REMOVE_DUPLICATES PREFIXES)
foreach(PREFIX IN LISTS PREFIXES)
  if(NOT EXISTS ${PREFIX})
    continue()
  endif()

  # bin
  link_files(GLOB ${PREFIX}/bin "*.dll" install/bin)
  link_files(GLOB ${PREFIX}/bin "*.exe" install/bin)

  # lib/mariadb
  link_files(GLOB_RECURSE ${PREFIX}/lib/mariadb "*.dll" install/bin)

  # lib other
  link_files(GLOB_RECURSE ${PREFIX}/lib "perl*/*" install/lib)
  link_files(GLOB_RECURSE ${PREFIX}/lib "python*/*" install/lib)

  # lib64
  if(EXISTS ${PREFIX}/lib64)
    link_files(GLOB_RECURSE ${PREFIX}/lib64 "perl*/*" install/lib)
    link_files(GLOB_RECURSE ${PREFIX}/lib64 "python*/*" install/lib)
  endif()

  # share
  link_files(GLOB_RECURSE ${PREFIX}/share "perl*/*" install/bin/data)
  link_files(GLOB_RECURSE ${PREFIX}/share "mythtv/*" install/bin/data)

  # qt
  if(EXISTS ${PREFIX}/qt)
    link_files(GLOB ${PREFIX}/qt/bin "*.dll" install/bin)
    link_files(GLOB_RECURSE ${PREFIX}/qt/plugins "*.dll" install/bin/plugins)
  endif()
endforeach()

#
# Pick up other libraries from the mingw sys-root.  Some of these are libraries
# that we didn't have to build because they were available in pre-built mingw
# packages.  Others are low level gcc/mingw libraries.
#
set(SYSROOT /usr/${TOOLCHAIN_PREFIX}/sys-root/mingw)
set(FILENAME_BASES
    # cmake-format: off

    # Libs we didn't have to compile
    crypto # openssl
    flac
    fontconfig
    freetype
    fribidi
    harfbuzz
    iconv
    icu
    libz
    mp3lame
    ogg
    Qt5
    ssl    # openssl
    tag
    vorbis
    zip
    zlib
    zstd

    # Other system libraries
    atomic
    bz2
    gcc_s_seh
    libpng16
    pthread
    ssp
    stdc++

    # Required by pre-built mingw libraries on Fedora. If not present
    # they won't be included.
    expat    # used by fontconfig
    glib     # used by harfbuzz
    intl     # used by glib
    libpcre2 # used by glib

    # cmake-format: on
)

function(find_names VARIABLE PATH SUFFIX)
  if(ARGC LESS 4)
    message(DEBUG "Not enough arguments to find_names")
  endif()
  set(NAMES ${ARGN})
  list(TRANSFORM NAMES PREPEND "${PATH}/*")
  list(TRANSFORM NAMES APPEND ${SUFFIX})
  file(GLOB FOUND ${NAMES})
  set(${VARIABLE}
      ${FOUND}
      PARENT_SCOPE)
endfunction()

# Copy dll files from the sysroot bin directory to our local install tree.
find_names(BINNAMES "${SYSROOT}/bin" "*.dll" ${FILENAME_BASES})
if(BINNAMES)
  file(COPY ${BINNAMES} DESTINATION install/bin)
endif()

#
# Cmake puts all DLL files into /bin. Move the plugins to the right directory.
#
link_files(GLOB ${PROJECT_BINARY_DIR}/install/lib/mythtv/plugins *.dll
           install/bin/plugins)

#
# Command to create the zip install file, dependent on any of the original files
# that were symlinked into the local install directory.
#
add_custom_command(
  OUTPUT MythTV_Windows.zip
  COMMAND zip -r9 ../MythTV_Windows.zip .
  DEPENDS ${ALLFILES}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/install
  USES_TERMINAL)

add_custom_target(ZipFile ALL DEPENDS MythTV_Windows.zip)

#
# Install at the top level.
#
install(FILES ${CMAKE_BINARY_DIR}/MythTV_Windows.zip
        DESTINATION ${SUPER_BINARY_DIR})
