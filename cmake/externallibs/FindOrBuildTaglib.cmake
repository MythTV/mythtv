#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Taglib
#
# cmake, C++ source only
#
function(find_or_build_taglib)
  pkg_check_modules(TAGLIB "taglib" QUIET IMPORTED_TARGET)
  if(TAGLIB_FOUND)
    list(GET TAGLIB_LINK_LIBRARIES 0 TAGLIB_LINK_LIBRARY)
    message(STATUS "Found taglib ${TAGLIB_VERSION} at ${TAGLIB_LINK_LIBRARY}")
    add_library(taglib ALIAS PkgConfig::TAGLIB)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(TAGLIB_VERSION "1.13.1")
  set(TAGLIB_PREFIX "taglib-${TAGLIB_VERSION}")
  set(TAGLIB_1.12_SHA256
      "7fccd07669a523b07a15bd24c8da1bbb92206cb19e9366c3692af3d79253b703")
  set(TAGLIB_1.13_SHA256
      "58f08b4db3dc31ed152c04896ee9172d22052bc7ef12888028c01d8b1d60ade0")
  set(TAGLIB_1.13.1_SHA256
      "c8da2b10f1bfec2cd7dbfcd33f4a2338db0765d851a50583d410bacf055cfd0b")
  ExternalProject_Add(
    taglib
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL http://taglib.org/releases/${TAGLIB_PREFIX}.tar.gz
    URL_HASH SHA256=${TAGLIB_${TAGLIB_VERSION}_SHA256}
    CMAKE_ARGS --no-warn-unused-cli ${CMDLINE_ARGS_LIBS} ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON -DBUILD_TESTING:BOOL=OFF
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ${after_libs})

  add_dependencies(external_libs taglib)

  message(STATUS "Will build taglib (${TAGLIB_VERSION}) ${PREBUILT_TAG}")
endfunction()
