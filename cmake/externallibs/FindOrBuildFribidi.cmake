#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# fribidi
#
# configure, C source
#
# If CMP0135 (new in cmake 3.24) is enabled, it screws up the logic that
# autotools uses to decide if files need to be rebuilt.  This will cause the
# build to fail.  The autoconf-madness step is an attempt to restore the proper
# ordering of timestamps to prevent this.
#
function(find_or_build_fribidi)
  pkg_check_modules(FRIBIDI "fribidi" QUIET IMPORTED_TARGET)
  if(FRIBIDI_FOUND)
    list(GET FRIBIDI_LINK_LIBRARIES 0 FRIBIDI_LINK_LIBRARY)
    message(
      STATUS "Found fribidi ${FRIBIDI_VERSION} at ${FRIBIDI_LINK_LIBRARY}")
    add_library(fribidi ALIAS PkgConfig::FRIBIDI)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(FRIBIDI_VERSION "1.0.13")
  set(FRIBIDI_PREFIX "fribidi-${FRIBIDI_VERSION}")
  set(FRIBIDI_1.0.12_SHA256
      "0cd233f97fc8c67bb3ac27ce8440def5d3ffacf516765b91c2cc654498293495")
  set(FRIBIDI_1.0.13_SHA256
      "7fa16c80c81bd622f7b198d31356da139cc318a63fc7761217af4130903f54a2")
  ExternalProject_Add(
    fribidi
    DOWNLOAD_DIR ${TARBALL_DIR}
    # DOWNLOAD_EXTRACT_TIMESTAMP ON
    URL https://github.com/fribidi/fribidi/releases/download/v${FRIBIDI_VERSION}/${FRIBIDI_PREFIX}.tar.xz
    URL_HASH SHA256=${FRIBIDI_${FRIBIDI_VERSION}_SHA256}
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_CONFIGURE_ENV} <SOURCE_DIR>/configure
      ${PLATFORM_BUILD_AND_HOST} ${CONFIGURE_VERBOSE_ARG}
      --prefix=${LIBS_INSTALL_PREFIX} --libdir=${LIBS_INSTALL_PREFIX}/lib
      --enable-shared=yes --enable-static=no --srcdir=<SOURCE_DIR>
    BUILD_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    DEPENDS ${after_libs})

  add_dependencies(external_libs fribidi)

  message(STATUS "Will build fribidi (${FRIBIDI_VERSION}) ${PREBUILT_TAG}")
endfunction()
