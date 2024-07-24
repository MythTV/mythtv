#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Only build the nv-codec-headers on Linux and Windows. On any other platform
# the FFmpeg configure script will disable nvdec, nvec, and ffnvcodec, so
# there's no point in setting up the headers.
#
if(NOT CMAKE_SYSTEM_NAME MATCHES "Linux|Windows" OR NOT CMAKE_SYSTEM_PROCESSOR
                                                    MATCHES "x86|aarch64")
  list(APPEND FF_ARGS --disable-nvdec --disable-nvenc --disable-ffnvcodec)
  return()
endif()

if(LIBS_USE_INSTALLED)
  pkg_check_modules(FFNVCODEC ffnvcodec QUIET IMPORTED_TARGET)
  if(TARGET PkgConfig::FFNVCODEC)
    message(STATUS "Found ffnvcodec")
    list(APPEND FF_ARGS --enable-nvdec --enable-nvenc --enable-ffnvcodec)
    return()
  endif()
endif()

if(ENABLE_STRICT_BUILD_ORDER)
  get_property(
    after_libs
    TARGET embedded_libs
    PROPERTY MANUALLY_ADDED_DEPENDENCIES)
endif()

ExternalProject_Add(
  nv-codec-headers
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mythtv/external/nv-codec-headers
  CMAKE_ARGS --no-warn-unused-cli ${CMDLINE_ARGS} ${PLATFORM_ARGS}
  CMAKE_CACHE_ARGS
    -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
    -DCMAKE_JOB_POOL_COMPILE:STRING=compile
    -DCMAKE_JOB_POOL_LINK:STRING=link
    -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
    -DPKG_CONFIG_LIBDIR:PATH=${PKG_CONFIG_LIBDIR}
    -DPKG_CONFIG_PATH:PATH=${PKG_CONFIG_PATH}
  BUILD_ALWAYS ${LIBS_ALWAYS_REBUILD}
  DEPENDS external_libs ${after_libs})

add_dependencies(embedded_libs nv-codec-headers)

# Update CMAKE_PREFIX_PATH so that FFmpeg can find the generated files.
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig")
list(APPEND ProjectDepends nv-codec-headers)
list(APPEND FF_ARGS --enable-nvdec --enable-nvenc --enable-ffnvcodec)
