#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# libbluray
#
# configure, C source
#
function(find_or_build_libbluray)
  pkg_check_modules(LIBBLURAY "libbluray" QUIET IMPORTED_TARGET)
  if(LIBBLURAY_FOUND)
    message(
      STATUS
        "Found libbluray ${LIBBLURAY_VERSION} at ${LIBBLURAY_LINK_LIBRARIES}")
    add_library(libbluray ALIAS PkgConfig::LIBBLURAY)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(LIBBLURAY_VERSION "1.3.4")
  set(LIBBLURAY_PREFIX "libbluray-${LIBBLURAY_VERSION}")
  set(LIBBLURAY_1.1.2_SHA256
      "a3dd452239b100dc9da0d01b30e1692693e2a332a7d29917bf84bb10ea7c0b42")
  set(LIBBLURAY_1.3.1_SHA256
      "c24b0f41c5b737bbb65c544fe63495637a771c10a519dfc802e769f112b43b75")
  set(LIBBLURAY_1.3.4_SHA256
      "478ffd68a0f5dde8ef6ca989b7f035b5a0a22c599142e5cd3ff7b03bbebe5f2b")
  ExternalProject_Add(
    libbluray
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL ftp://ftp.videolan.org/pub/videolan/libbluray/${LIBBLURAY_VERSION}/${LIBBLURAY_PREFIX}.tar.bz2
    URL_HASH SHA256=${LIBBLURAY_${LIBBLURAY_VERSION}_SHA256}
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${LIBBLURAY_CONFIGURE_ENV} <SOURCE_DIR>/configure
      ${PLATFORM_BUILD_AND_HOST} ${CONFIGURE_VERBOSE_ARG}
      --prefix=${LIBS_INSTALL_PREFIX} --enable-shared=yes --enable-static=no
      --without-fontconfig --without-freetype --disable-silent-rules
      --disable-examples --disable-bdjava-jar --disable-dependency-tracking
      --disable-doxygen-doc --disable-doxygen-dot --disable-doxygen-html
      --disable-doxygen-ps --disable-doxygen-pdf --srcdir=<SOURCE_DIR>
    BUILD_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    DEPENDS ${after_libs})

  add_dependencies(external_libs libbluray)

  message(STATUS "Will build libbluray (${LIBBLURAY_VERSION})")
endfunction()
