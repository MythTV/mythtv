#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# iconv
#
# configure, C only
#
function(find_or_build_iconv)
  find_library(ICONV NAMES iconv NO_CACHE)
  if(ICONV)
    # Version not in header file. No pkgconfig file.
    message(STATUS "Found iconv at ${ICONV}")
    add_library(iconv SHARED IMPORTED)
    return()
  endif()

  if(ANDROID AND ANDROID_MIN_SDK_VERSION GREATER_EQUAL 28)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(ICONV_VERSION "1.16")
  # Compiling 1.17 needs work. Set __USE_FORTIFY_LEVEL??
  set(ICONV_PREFIX "libiconv-${ICONV_VERSION}")
  set(ICONV_1.16_SHA256
      "e6a1b1b589654277ee790cce3734f07876ac4ccfaecbee8afa0b649cf529cc04")
  set(ICONV_1.17_SHA256
      "8f74213b56238c85a50a5329f77e06198771e70dd9a739779f4c02f65d971313")
  ExternalProject_Add(
    iconv
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://ftp.gnu.org/pub/gnu/libiconv/${ICONV_PREFIX}.tar.gz
    URL_HASH SHA256=${ICONV_${ICONV_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 <
                  ${PROJECT_SOURCE_DIR}/patches/${ICONV_PREFIX}.patch
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_CONFIGURE_ENV} <SOURCE_DIR>/configure
      ${PLATFORM_BUILD_AND_HOST} --prefix=${LIBS_INSTALL_PREFIX}
      --enable-shared=yes --enable-static=no --srcdir=<SOURCE_DIR>
    BUILD_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    DEPENDS ${after_libs})

  add_dependencies(external_libs iconv)

  message(STATUS "Will build iconv (${ICONV_VERSION}) ${PREBUILT_TAG}")
endfunction()
