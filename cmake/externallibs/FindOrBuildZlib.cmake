#
# Copyright (C) 2022-2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# zlib
#
# cmake, C source
#
function(find_or_build_zlib)
  #
  # Zlib is a PITA on android.  The libz.a and libz.so files are in different
  # directories, and earlier libz versions are missing a symbol needed by the
  # libpng library.  Always build our own version.
  #
  # This is also a total PITA trying to use find_package or pkg_check_modules
  # this early in the build, as it always does the wrong thing.  Even setting
  # ZLIB_ROOT doesn't make it work properly.  Manually search for the library
  # here so we can explicitly search for the version that we built.
  #
  find_library(ZLIB z NO_DEFAULT_PATH HINTS ${ZLIB_ROOT}/lib)
  if(ZLIB)
    find_file(ZLIBH zlib.h NO_DEFAULT_PATH HINTS ${ZLIB_ROOT}/include)
    get_define(${ZLIBH} ZLIB_VERSION "^#define +ZLIB_VERSION +\"([^\"]+)\" *$")
    message(STATUS "Found zlib ${ZLIB_VERSION} at ${ZLIB}")
    add_library(zlib OBJECT IMPORTED)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(ZLIB_VERSION "1.3")
  set(ZLIB_PREFIX "zlib-${ZLIB_VERSION}")
  set(ZLIB_1.2.13_SHA256
      "1525952a0a567581792613a9723333d7f8cc20b87a81f920fb8bc7e3f2251428")
  set(ZLIB_1.3_SHA256
      "b5b06d60ce49c8ba700e0ba517fa07de80b5d4628a037f4be8ad16955be7a7c0")
  ExternalProject_Add(
    zlib
    DOWNLOAD_DIR ${TARBALL_DIR}
    DOWNLOAD_NAME ${ZLIB_PREFIX}.tar.gz
    URL https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_VERSION}.tar.gz
    URL_HASH SHA256=${ZLIB_${ZLIB_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 < ${PROJECT_SOURCE_DIR}/patches/${ZLIB_PREFIX}.patch
    CMAKE_ARGS --no-warn-unused-cli ${CMDLINE_ARGS_LIBS} ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ${after_libs})

  if(ANDROID)
    # Generate a zlib.pc file now (instead of waiting to build zlib) so that
    # later packages will configure properly.
    message(STATUS "Created file ${LIBS_INSTALL_PREFIX}/lib/pkgconfig/zlib.pc")
    configure_file(${PROJECT_SOURCE_DIR}/cmake/files/android/zlib.pc.in
                   ${LIBS_INSTALL_PREFIX}/lib/pkgconfig/zlib.pc @ONLY)
  endif()

  add_dependencies(external_libs zlib)

  message(STATUS "Will build zlib (${ZLIB_VERSION}) ${PREBUILT_TAG}")
endfunction()
