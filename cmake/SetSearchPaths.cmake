#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(CMAKE_CROSSCOMPILING)
  # See https://cmake.org/cmake/help/latest/command/find_package.html
  #
  # Set roots within which each of the below will be searched. The content of
  # CMAKE_SYSROOT doesn't need to be included here.
  # ~~~
  # set(CMAKE_FIND_ROOT_PATH ${CMAKE_INSTALL_PREFIX})
  # ~~~

  # Set list of prefixes to be tried with each root. Item #2 says this is
  # intended for use with -DVAR=value. The alternative would be to use
  # CMAKE_SYSTEM_PREFIX_PATH, but documentation for that variable says its *not*
  # to be modified by the project.
  #
  set(CMAKE_PREFIX_PATH
      # Handle sysroot that doesn't have a 'usr' directory, which is
      # where cmake normally looks for things.
      /
      # Don't remember why this one is here.
      /usr
      # Add for finding cmake modules
      /lib/cmake
      # Find various qt files and qt cmake modules
      /qt
      /qt/lib/cmake)
  # Conditionally add android per-arch lib directory
  if(CMAKE_ANDROID_ARCH_TRIPLE)
    list(APPEND CMAKE_PREFIX_PATH
         /usr/lib/${CMAKE_ANDROID_ARCH_TRIPLE}/${CMAKE_SYSTEM_VERSION})
  endif()
  set(CMAKE_PROGRAM_PATH /usr /qt)

  # Item 3: Disable the default cmake environment paths
  set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH OFF)

  # Item 4: Hints option

  # Item 5: Disable the default system environment paths
  set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF)

  # Item 6: Paths Option

  # Item 7: Disable the default system paths
  set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)

  # ~~~
  # ONLY means only search in CMAKE_FIND_ROOT_PATH
  # NEVER means only search in host system root
  # BOTH means search both
  # ~~~
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
endif()

#
# Tell the cmake "find" functions to always search for packages in our installed
# directories.
#
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_INSTALL_PREFIX})
if(NOT LIBS_INSTALL_PREFIX STREQUAL CMAKE_INSTALL_PREFIX)
  list(APPEND CMAKE_FIND_ROOT_PATH ${LIBS_INSTALL_PREFIX})
  list(APPEND CMAKE_MODULE_PATH ${LIBS_INSTALL_PREFIX}/lib/cmake)
  list(APPEND CMAKE_MODULE_PATH ${LIBS_INSTALL_PREFIX}/qt/lib/cmake)
endif()

#
# Tell pkg-config to always search for packages in our installed directories.
#
list(APPEND PKG_CONFIG_PATH ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig
     ${CMAKE_INSTALL_PREFIX}/lib64/pkgconfig)
if(NOT LIBS_INSTALL_PREFIX STREQUAL CMAKE_INSTALL_PREFIX)
  list(APPEND PKG_CONFIG_PATH ${LIBS_INSTALL_PREFIX}/lib/pkgconfig
       ${LIBS_INSTALL_PREFIX}/lib64/pkgconfig)
endif()

#
# Get any platform specfic customizations
#
if(CMAKE_CROSSCOMPILING)
  include(platform/${CMAKE_SYSTEM_NAME}CrossSearchPaths OPTIONAL)
else()
  include(platform/${CMAKE_SYSTEM_NAME}SearchPaths OPTIONAL)
endif()

#
# Clean up the lists. Duplicate entries waste time.
#
list(REMOVE_DUPLICATES CMAKE_FIND_ROOT_PATH)
list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)
list(REMOVE_DUPLICATES PKG_CONFIG_PATH)
