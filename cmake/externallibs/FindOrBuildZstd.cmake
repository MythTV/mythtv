#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# libzstd
#
# configure, C source
#
function(find_or_build_zstd)
  pkg_check_modules(ZSTD "zstd" QUIET IMPORTED_TARGET)
  if(ZSTD_FOUND)
    message(STATUS "Found zstd ${ZSTD_VERSION} at ${ZSTD_LINK_LIBRARIES}")
    add_library(zstd ALIAS PkgConfig::ZSTD)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(ZSTD_VERSION "1.5.7")
  set(ZSTD_PREFIX "zstd-${ZSTD_VERSION}")
  set(ZSTD_1.5.7_SHA256
      "37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3")
  ExternalProject_Add(
    zstd
    DOWNLOAD_DIR ${TARBALL_DIR}
    DOWNLOAD_NAME ${ZSTD_PREFIX}.tar.gz
    URL https://github.com/facebook/zstd/archive/refs/tags/v${ZSTD_VERSION}.tar.gz
    URL_HASH SHA256=${ZSTD_${ZSTD_VERSION}_SHA256}
#    PATCH_COMMAND patch -p1 <
#                  ${PROJECT_SOURCE_DIR}/patches/${ZSTD_PREFIX}.patch
    CONFIGURE_COMMAND echo > /dev/null
    BUILD_IN_SOURCE ON
    BUILD_COMMAND ${CMAKE_COMMAND} -E env ${PLATFORM_CONFIGURE_ENV} ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    INSTALL_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG} prefix=${LIBS_INSTALL_PREFIX} install
    DEPENDS ${after_libs})

  add_dependencies(external_libs zstd)

  message(STATUS "Will build libzstd (${ZSTD_VERSION}) ${PREBUILT_TAG}")
endfunction()
