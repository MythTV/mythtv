#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# lzo
#
# configure, C source
#
function(find_or_build_lzo)
  pkg_check_modules(LIBLZO "lzo2" QUIET IMPORTED_TARGET)
  if(LIBLZO_FOUND)
    message(STATUS "Found liblzo ${LIBLZO_VERSION} at ${LIBLZO_LINK_LIBRARIES}")
    add_library(liblzo ALIAS PkgConfig::LIBLZO)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(LZO_VERSION "2.10")
  set(LZO_PREFIX "lzo-${LZO_VERSION}")
  set(LZO_2.10_SHA256
      "c0f892943208266f9b6543b3ae308fab6284c5c90e627931446fb49b4221a072")
  ExternalProject_Add(
    liblzo
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL http://www.oberhumer.com/opensource/lzo/download/${LZO_PREFIX}.tar.gz
    URL_HASH SHA256=${LZO_${LZO_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 < ${PROJECT_SOURCE_DIR}/patches/${LZO_PREFIX}.patch
    CMAKE_ARGS --no-warn-unused-cli ${CMDLINE_ARGS_LIBS} ${PLATFORM_ARGS}
               -DENABLE_SHARED:BOOL=ON -DENABLE_STATIC:BOOL=OFF
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ${after_libs})

  add_dependencies(external_libs liblzo)

  message(STATUS "Will build lzo (${LZO_VERSION})")
endfunction()
