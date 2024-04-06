#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# fontconfig
#
# configure, C sourcs
#
function(find_or_build_fontconfig)
  pkg_check_modules(FONTCONFIG "fontconfig" QUIET IMPORTED_TARGET)
  if(FONTCONFIG_FOUND)
    list(GET FONTCONFIG_LINK_LIBRARIES 0 FONTCONFIG_LINK_LIBRARY)
    message(
      STATUS
        "Found fontconfig ${FONTCONFIG_VERSION} at ${FONTCONFIG_LINK_LIBRARY}")
    add_library(fontconfig ALIAS PkgConfig::FONTCONFIG)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(FONTCONFIG_VERSION "2.14.1")
  set(FONTCONFIG_PREFIX "fontconfig-${FONTCONFIG_VERSION}")
  set(FONTCONFIG_2.13.92_SHA256
      "506e61283878c1726550bc94f2af26168f1e9f2106eac77eaaf0b2cdfad66e4e")
  set(FONTCONFIG_2.14.1_SHA256
      "298e883f6e11d2c5e6d53c8a8394de58d563902cfab934e6be12fb5a5f361ef0")
  set(FONTCONFIG_2.14.2_SHA256
      "dba695b57bce15023d2ceedef82062c2b925e51f5d4cc4aef736cf13f60a468b")
  ExternalProject_Add(
    fontconfig
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://www.freedesktop.org/software/fontconfig/release/${FONTCONFIG_PREFIX}.tar.xz
    URL_HASH SHA256=${FONTCONFIG_${FONTCONFIG_VERSION}_SHA256}
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_CONFIGURE_ENV} <SOURCE_DIR>/configure
      ${PLATFORM_BUILD_AND_HOST} --prefix=${LIBS_INSTALL_PREFIX}
      ${CONFIGURE_VERBOSE_ARG} --enable-shared=yes --enable-static=no
      --disable-docs --disable-dependency-tracking --enable-libxml2
      --srcdir=<SOURCE_DIR>
    BUILD_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    DEPENDS freetype libxml2 ${after_libs})
  ExternalProject_Add_Step(
    fontconfig post-install
    DEPENDEES install
    COMMAND ${CMAKE_COMMAND} -E rm ${LIBS_INSTALL_PREFIX}/lib/libfontconfig.la)

  add_dependencies(external_libs fontconfig)

  message(
    STATUS "Will build fontconfig (${FONTCONFIG_VERSION}) ${PREBUILT_TAG}")
endfunction()
