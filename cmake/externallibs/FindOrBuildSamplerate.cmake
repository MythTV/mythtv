#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# libsamplerate
#
# configure, C source
#
# If CMP0135 (new in cmake 3.24) is enabled, it screws up the logic that
# autotools uses to decide if files need to be rebuilt.  This will cause the
# build to fail.  The autoconf-madness step is an attempt to restore the proper
# ordering of timestamps to prevent this.
#
function(find_or_build_samplerate)
  pkg_check_modules(SAMPLERATE "samplerate" QUIET IMPORTED_TARGET)
  if(SAMPLERATE_FOUND)
    message(
      STATUS
        "Found samplerate ${SAMPLERATE_VERSION} at ${SAMPLERATE_LINK_LIBRARIES}"
    )
    add_library(samplerate ALIAS PkgConfig::SAMPLERATE)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(SAMPLERATE_VERSION "0.1.9")
  set(SAMPLERATE_PREFIX "libsamplerate-${SAMPLERATE_VERSION}")
  set(SAMPLERATE_0.1.9_SHA256
      "0a7eb168e2f21353fb6d84da152e4512126f7dc48ccb0be80578c565413444c1")
  ExternalProject_Add(
    samplerate
    DOWNLOAD_DIR ${TARBALL_DIR}
    # DOWNLOAD_EXTRACT_TIMESTAMP ON
    URL http://www.mega-nerd.com/SRC/${SAMPLERATE_PREFIX}.tar.gz
    URL_HASH SHA256=${SAMPLERATE_${SAMPLERATE_VERSION}_SHA256}
    PATCH_COMMAND cp ${PROJECT_SOURCE_DIR}/patches/autoconf/config.sub
                  <SOURCE_DIR>/Cfg/config.sub
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_CONFIGURE_ENV} <SOURCE_DIR>/configure
      ${PLATFORM_BUILD_AND_HOST} ${CONFIGURE_VERBOSE_ARG}
      --prefix=${LIBS_INSTALL_PREFIX} --libdir=${LIBS_INSTALL_PREFIX}/lib
      --enable-shared=yes --enable-static=no --srcdir=<SOURCE_DIR>
    BUILD_COMMAND
    COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    DEPENDS ${after_libs})

  add_dependencies(external_libs samplerate)

  message(STATUS "Will build samplerate (${SAMPLERATE_VERSION})")
endfunction()
