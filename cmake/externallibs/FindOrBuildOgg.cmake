#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# ogg
#
# cmake, C source
#
function(find_or_build_ogg)
  pkg_check_modules(OGG "ogg" QUIET IMPORTED_TARGET)
  if(OGG_FOUND)
    message(STATUS "Found ogg ${OGG_VERSION} at ${OGG_LINK_LIBRARIES}")
    add_library(ogg ALIAS PkgConfig::OGG)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(OGG_VERSION "1.3.5")
  set(OGG_PREFIX "libogg-${OGG_VERSION}")
  set(OGG_1.3.5_SHA256
      "c4d91be36fc8e54deae7575241e03f4211eb102afb3fc0775fbbc1b740016705")
  ExternalProject_Add(
    ogg
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://ftp.osuosl.org/pub/xiph/releases/ogg/${OGG_PREFIX}.tar.xz
    URL_HASH SHA256=${OGG_${OGG_VERSION}_SHA256}
    CMAKE_ARGS --no-warn-unused-cli ${CMDLINE_ARGS_LIBS} ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON -DINSTALL_DOCS:BOOL=OFF
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ${after_libs})

  add_dependencies(external_libs ogg)

  message(STATUS "Will build ogg (${OGG_VERSION}) ${PREBUILT_TAG}")
endfunction()
