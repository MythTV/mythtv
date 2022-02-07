#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# dlfcn
#
# cmake, C source
#
function(find_or_build_dlfcn)
  find_library(DLFCN NAMES dl NO_CACHE)
  if(DLFCN)
    message(STATUS "Found dlfcn at ${DLFCN}")
    add_library(fdlfcn SHARED IMPORTED)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(DLFCN_VERSION "1.4.1")
  set(DLFCN_PREFIX "dlfcn-win32-${DLFCN_VERSION}")
  set(DLFCN_1.3.1_SHA256
      "f7248a8baeb79d9bcd5f702cc08a777431708758e70d1730b59674c5e795e88a")
  set(DLFCN_1.4.1_SHA256
      "30a9f72bdf674857899eb7e553df1f0d362c5da2a576ae51f886e1171fbdb399")
  ExternalProject_Add(
    dlfcn
    DOWNLOAD_DIR ${TARBALL_DIR}
    DOWNLOAD_NAME ${DLFCN_PREFIX}.tar.gz
    URL https://github.com/dlfcn-win32/dlfcn-win32/archive/refs/tags/v${DLFCN_VERSION}.tar.gz
    URL_HASH SHA256=${DLFCN_${DLFCN_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 <
                  ${PROJECT_SOURCE_DIR}/patches/${DLFCN_PREFIX}.patch
    CMAKE_ARGS --no-warn-unused-cli ${CMDLINE_ARGS_LIBS} ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON
    DEPENDS ${after_libs})

  add_dependencies(external_libs dlfcn)

  message(STATUS "Will build dlfcn (${DLFCN_VERSION})")
endfunction()
