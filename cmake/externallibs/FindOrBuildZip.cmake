#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# libzip
#
# configure, C source
#
function(find_or_build_libzip)
  pkg_check_modules(LIBZIP "libzip" QUIET IMPORTED_TARGET)
  if(LIBZIP_FOUND)
    message(STATUS "Found libzip ${LIBZIP_VERSION} at ${LIBZIP_LINK_LIBRARIES}")
    add_library(libzip ALIAS PkgConfig::LIBZIP)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(LIBZIP_VERSION "1.10.1")
  set(LIBZIP_PREFIX "libzip-${LIBZIP_VERSION}")
  set(LIBZIP_1.8.0_SHA256
      "f0763bda24ba947e80430be787c4b068d8b6aa6027a26a19923f0acfa3dac97e")
  set(LIBZIP_1.9.2_SHA256
      "c93e9852b7b2dc931197831438fee5295976ee0ba24f8524a8907be5c2ba5937")
  set(LIBZIP_1.10.1_SHA256
      "dc3c8d5b4c8bbd09626864f6bcf93de701540f761d76b85d7c7d710f4bd90318")
  ExternalProject_Add(
    libzip
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://libzip.org/download/${LIBZIP_PREFIX}.tar.xz
    URL_HASH SHA256=${LIBZIP_${LIBZIP_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 <
                  ${PROJECT_SOURCE_DIR}/patches/${LIBZIP_PREFIX}.patch
    CMAKE_ARGS --no-warn-unused-cli
               ${CMDLINE_ARGS_LIBS}
               ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON
               -DBUILD_TOOLS:BOOL=OFF
               -DBUILD_REGRESS:BOOL=OFF
               -DBUILD_EXAMPLES:BOOL=OFF
               -DBUILD_DOC:BOOL=OFF
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS zlib ${after_libs})

  add_dependencies(external_libs libzip)

  message(STATUS "Will build libzip (${LIBZIP_VERSION}) ${PREBUILT_TAG}")
endfunction()
