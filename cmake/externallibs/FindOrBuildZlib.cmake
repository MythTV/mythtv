#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# zlib
#
# cmake, C source
#
function(find_or_build_zlib)
  pkg_check_modules(ZLIB "zlib" QUIET IMPORTED_TARGET)
  if(ZLIB_FOUND)
    message(STATUS "Found zlib ${ZLIB_VERSION} at ${ZLIB_LINK_LIBRARIES}")
    add_library(zlib ALIAS PkgConfig::ZLIB)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(ZLIB_VERSION "1.2.13")
  set(ZLIB_VERSION "1.3")
  set(ZLIB_PREFIX "zlib-${ZLIB_VERSION}")
  set(ZLIB_1.2.13_SHA256
      "1525952a0a567581792613a9723333d7f8cc20b87a81f920fb8bc7e3f2251428")
  set(ZLIB_1.3_SHA256
      "b5b06d60ce49c8ba700e0ba517fa07de80b5d4628a037f4be8ad16955be7a7c0")
  if(ANDROID)
    message(FATAL_ERROR "Zlib should be built into the android sdk.")
  elseif(WIN32)
    ExternalProject_Add(
      zlib
      DOWNLOAD_DIR ${TARBALL_DIR}
      DOWNLOAD_NAME ${ZLIB_PREFIX}.tar.gz
      URL https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_VERSION}.tar.gz
      URL_HASH SHA256=${ZLIB_${ZLIB_VERSION}_SHA256}
      PATCH_COMMAND patch -p1 <
                    ${PROJECT_SOURCE_DIR}/patches/${ZLIB_PREFIX}.patch
      CONFIGURE_COMMAND echo
      BUILD_IN_SOURCE TRUE
      BUILD_COMMAND ${MAKE_EXECUTABLE} -f win32/Makefile.gcc
                    PREFIX=/usr/bin/${TOOLCHAIN_PREFIX}-
      INSTALL_COMMAND
        ${MAKE_EXECUTABLE} -f win32/Makefile.gcc install
        BINARY_PATH=${LIBS_INSTALL_PREFIX}/bin
        INCLUDE_PATH=${LIBS_INSTALL_PREFIX}/include
        LIBRARY_PATH=${LIBS_INSTALL_PREFIX}/lib SHARED_MODE=1
      DEPENDS ${after_libs})
  else()
    message(FATAL_ERROR "Don't know how to build zlib for this target.")
  endif()

  add_dependencies(external_libs zlib)

  message(STATUS "Will build zlib (${ZLIB_VERSION}) ${PREBUILT_TAG}")
endfunction()
