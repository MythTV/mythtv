#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# openssl
#
# configure, C and C++ source
#
# Defining __ANDROID_API__ or __ANDROID_MIN_SDK_VERSION__ results in compilation
# warnings about the symbol being redefined. Leaving the define out results in
# OpenSSL always choosing the highest available SDK version. The chosen solution
# was to patch the 15-android-conf to require passing the correct SDK version in
# an environment variable.
#
function(find_or_build_openssl)
  pkg_check_modules(OPENSSL "openssl" QUIET IMPORTED_TARGET)
  if(OPENSSL_FOUND)
    list(GET OPENSSL_LINK_LIBRARIES 0 OPENSSL_LINK_LIBRARY)
    message(
      STATUS "Found openssl ${OPENSSL_VERSION} at ${OPENSSL_LINK_LIBRARY}")
    add_library(openssl ALIAS PkgConfig::OPENSSL)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(OPENSSL_VERSION "3.1.4")
  set(OPENSSL_PREFIX "openssl-${OPENSSL_VERSION}")
  set(OPENSSL_3.0.10_SHA256
      "1761d4f5b13a1028b9b6f3d4b8e17feb0cedc9370f6afe61d7193d2cdce83323")
  set(OPENSSL_3.1.4_SHA256
      "840af5366ab9b522bde525826be3ef0fb0af81c6a9ebd84caa600fea1731eee3")
  set(OPENSSL_3.2.0_SHA256
      "14c826f07c7e433706fb5c69fa9e25dab95684844b4c962a2cf1bf183eb4690e")
  ExternalProject_Add(
    openssl
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://www.openssl.org/source/${OPENSSL_PREFIX}.tar.gz
    URL_HASH SHA256=${OPENSSL_${OPENSSL_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 <
                  ${PROJECT_SOURCE_DIR}/patches/${OPENSSL_PREFIX}.patch
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${OPENSSL_PLATFORM_CONFIG_ENV}
      <SOURCE_DIR>/Configure --prefix=${LIBS_INSTALL_PREFIX} --libdir=lib
      ${OPENSSL_PLATFORM_CONFIG_ARGS} shared no-tests
    BUILD_COMMAND ${CMAKE_COMMAND} -E env ${OPENSSL_PLATFORM_BUILD_ARGS}
                  ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    INSTALL_COMMAND ${MAKE_EXECUTABLE} install_sw
    USES_TERMINAL_BUILD ON
    DEPENDS ${after_libs})

  add_dependencies(external_libs openssl)

  message(STATUS "Will build openssl (${OPENSSL_VERSION}) ${PREBUILT_TAG}")
endfunction()
