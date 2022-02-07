#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# flac
#
# cmake >= 1.3.4, C and C++ source
#
function(find_or_build_flac)
  pkg_check_modules(FLAC "flac" QUIET IMPORTED_TARGET)
  if(FLAC_FOUND)
    message(STATUS "Found flac ${FLAC_VERSION} at ${FLAC_LINK_LIBRARIES}")
    add_library(flac ALIAS PkgConfig::FLAC)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(FLAC_VERSION "1.4.3")
  set(FLAC_PREFIX "flac-${FLAC_VERSION}")
  set(FLAC_1.3.4_SHA256
      "8ff0607e75a322dd7cd6ec48f4f225471404ae2730d0ea945127b1355155e737")
  set(FLAC_1.4.3_SHA256
      "6c58e69cd22348f441b861092b825e591d0b822e106de6eb0ee4d05d27205b70")
  ExternalProject_Add(
    flac
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://ftp.osuosl.org/pub/xiph/releases/flac/${FLAC_PREFIX}.tar.xz
    URL_HASH SHA256=${FLAC_${FLAC_VERSION}_SHA256}
    CMAKE_ARGS --no-warn-unused-cli
               ${CMDLINE_ARGS_LIBS}
               ${PLATFORM_ARGS}
               -DBUILD_PROGRAMS:BOOL=OFF
               -DBUILD_EXAMPLES:BOOL=OFF
               -DBUILD_TESTING:BOOL=OFF
               -DBUILD_DOCS:BOOL=OFF
               -DINSTALL_MANPAGES:BOOL=OFF
               -DBUILD_SHARED_LIBS:BOOL=ON
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ogg ${after_libs})

  add_dependencies(external_libs flac)

  message(STATUS "Will build flac (${FLAC_VERSION}) ${PREBUILT_TAG}")
endfunction()
