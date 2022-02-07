#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# vorbis
#
# cmake >= 1.3.7 cmake, C source
#
function(find_or_build_vorbis)
  pkg_check_modules(VORBIS "vorbis" QUIET IMPORTED_TARGET)
  if(VORBIS_FOUND)
    message(STATUS "Found vorbis ${VORBIS_VERSION} at ${VORBIS_LINK_LIBRARIES}")
    add_library(vorbis ALIAS PkgConfig::VORBIS)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(VORBIS_VERSION "1.3.7")
  set(VORBIS_PREFIX "libvorbis-${VORBIS_VERSION}")
  set(VORBIS_1.3.7_SHA256
      "b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b")
  ExternalProject_Add(
    vorbis
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://ftp.osuosl.org/pub/xiph/releases/vorbis/${VORBIS_PREFIX}.tar.xz
    URL_HASH SHA256=${VORBIS_${VORBIS_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 <
                  ${PROJECT_SOURCE_DIR}/patches/${VORBIS_PREFIX}.patch
    CMAKE_ARGS --no-warn-unused-cli
               ${CMDLINE_ARGS_LIBS}
               ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON
               -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF
               -DCMAKE_FIND_USE_INSTALL_PREFIX:BOOL=ON
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ogg ${after_libs})

  add_dependencies(external_libs vorbis)

  message(STATUS "Will build vorbis (${VORBIS_VERSION}) ${PREBUILT_TAG}")
endfunction()
