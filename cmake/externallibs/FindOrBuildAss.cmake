#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# libass
#
# configure, C source only
#
# 0.15 added a hard requirement for harfbuzz.
#
function(find_or_build_libass)
  pkg_check_modules(LIBASS "libass" QUIET IMPORTED_TARGET)
  if(LIBASS_FOUND)
    message(STATUS "Found libass ${LIBASS_VERSION} at ${LIBASS_LINK_LIBRARIES}")
    add_library(libass ALIAS PkgConfig::LIBASS)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(LIBASS_VERSION "0.17.1")
  set(LIBASS_PREFIX "libass-${LIBASS_VERSION}")
  set(LIBASS_0.16.0_SHA256
      "5dbde9e22339119cf8eed59eea6c623a0746ef5a90b689e68a090109078e3c08")
  set(LIBASS_0.17.1_SHA256
      "f0da0bbfba476c16ae3e1cfd862256d30915911f7abaa1b16ce62ee653192784")
  ExternalProject_Add(
    libass
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://github.com/libass/libass/releases/download/${LIBASS_VERSION}/${LIBASS_PREFIX}.tar.xz
    URL_HASH SHA256=${LIBASS_${LIBASS_VERSION}_SHA256}
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_CONFIGURE_ENV}
      "LDFLAGS=-L${LIBS_INSTALL_PREFIX}/lib" <SOURCE_DIR>/configure
      ${PLATFORM_BUILD_AND_HOST} ${CONFIGURE_VERBOSE_ARG}
      --prefix=${LIBS_INSTALL_PREFIX} --enable-shared=yes --enable-static=no
      --disable-dependency-tracking --disable-require-system-font-provider
      --srcdir=<SOURCE_DIR>
    BUILD_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    DEPENDS fontconfig freetype fribidi harfbuzz libxml2 ${after_libs})

  add_dependencies(external_libs libass)

  message(STATUS "Will build libass (${LIBASS_VERSION})")
endfunction()
