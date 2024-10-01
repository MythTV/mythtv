#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# libxml2
#
# cmake >= 2.10, C source
#
# needs zlib, iconv. Both supplied by android ndk
#
function(find_or_build_libxml2)
  pkg_check_modules(LIBXML2 "libxml-2.0" QUIET IMPORTED_TARGET)
  if(LIBXML2_FOUND)
    message(
      STATUS "Found libxml2 ${LIBXML2_VERSION} at ${LIBXML2_LINK_LIBRARIES}")
    add_library(libxml2 ALIAS PkgConfig::LIBXML2)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(LIBXML2_VERSION "2.11.5")
  string(REGEX MATCH "^([0-9]+\.[0-9]+)" LIBXML2_MAJMIN ${LIBXML2_VERSION})
  set(LIBXML2_PREFIX "libxml2-${LIBXML2_VERSION}")
  set(LIBXML2_2.10.3_SHA256
      "5d2cc3d78bec3dbe212a9d7fa629ada25a7da928af432c93060ff5c17ee28a9c")
  set(LIBXML2_2.11.5_SHA256
      "3727b078c360ec69fa869de14bd6f75d7ee8d36987b071e6928d4720a28df3a6")
  ExternalProject_Add(
    libxml2
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://download.gnome.org/sources/libxml2/${LIBXML2_MAJMIN}/${LIBXML2_PREFIX}.tar.xz
    URL_HASH SHA256=${LIBXML2_${LIBXML2_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 <
                  ${PROJECT_SOURCE_DIR}/patches/${LIBXML2_PREFIX}.patch
    CMAKE_ARGS --no-warn-unused-cli
               ${CMDLINE_ARGS_LIBS}
               ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON
               -DLIBXML2_WITH_LZMA:BOOL=OFF
               -DLIBXML2_WITH_PYTHON:BOOL=OFF
               -DLIBXML2_WITH_PROGRAMS:BOOL=OFF
               -DLIBXML2_WITH_TESTS:BOOL=OFF
               ${XML2_PLATFORM_ARGS}
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ${after_libs})

  add_dependencies(external_libs libxml2)

  message(STATUS "Will build libxml2 (${LIBXML2_VERSION}) ${PREBUILT_TAG}")
endfunction()
