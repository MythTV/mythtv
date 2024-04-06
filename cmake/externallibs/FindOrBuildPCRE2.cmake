#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# pcre2
#
# configure, C source
#
function(find_or_build_pcre2)
  pkg_check_modules(LIBPCRE2 "libpcre2-32" QUIET IMPORTED_TARGET)
  if(LIBPCRE2_FOUND)
    message(
      STATUS "Found libpcre2 ${LIBPCRE2_VERSION} at ${LIBPCRE2_LINK_LIBRARIES}")
    add_library(libpcre2 ALIAS PkgConfig::LIBPCRE2)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(PCRE2_VERSION "10.42")
  set(PCRE2_PREFIX "pcre2-${PCRE2_VERSION}")
  set(PCRE2_10.42_SHA256
      "8d36cd8cb6ea2a4c2bb358ff6411b0c788633a2a45dabbf1aeb4b701d1b5e840")
  ExternalProject_Add(
    libpcre2
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://github.com/PCRE2Project/pcre2/releases/download/${PCRE2_PREFIX}/${PCRE2_PREFIX}.tar.bz2
    URL_HASH SHA256=${PCRE2_${PCRE2_VERSION}_SHA256}
    # PATCH_COMMAND patch -p1 <
    # ${PROJECT_SOURCE_DIR}/patches/${PCRE2_PREFIX}.patch
    CMAKE_ARGS --no-warn-unused-cli
               ${CMDLINE_ARGS_LIBS}
               ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON
               -DBUILD_STATIC_LIBS:BOOL=OFF
               -DPCRE2_BUILD_PCRE2_16=ON
               -DPCRE2_BUILD_PCRE2_32=ON
               -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ${after_libs})

  add_dependencies(external_libs libpcre2)

  message(STATUS "Will build pcre2 (${PCRE2_VERSION})")
endfunction()
