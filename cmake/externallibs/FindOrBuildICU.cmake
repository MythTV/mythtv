#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# Windows builds of Qt use the Windows API instead of ICU.
# https://lists.qt-project.org/pipermail/development/2016-May/025797.html We
# create a library here so that harfbuzz can declare a dependency on ICU without
# having to care whether it is building for android or windows.
if(WIN32)
  add_library(icu SHARED IMPORTED GLOBAL)
  function(find_or_build_icu)

  endfunction()
  return()
endif()

#
# ICU
#
# Configure C and C++
#
# ICU must be built twice.  A native version must be built, and then that
# version can be used to build the cross-compiled version.
#
function(find_or_build_icu)
  pkg_check_modules(ICU "icu-i18n" QUIET IMPORTED_TARGET)
  if(ICU_FOUND)
    list(GET ICU_LINK_LIBRARIES 0 ICU_LINK_LIBRARY)
    message(STATUS "Found icu ${ICU_VERSION} at ${ICU_LINK_LIBRARY}")
    string(REGEX MATCH "^([0-9]+)" ICU_VERSION_MAJOR ${ICU_VERSION})
    set(ICU_VERSION_MAJOR
        ${ICU_VERSION_MAJOR}
        PARENT_SCOPE)
    add_library(icu ALIAS PkgConfig::ICU)
    return()
  endif()

  #
  # Common setup
  #
  set(ICU_VERSION "73.2")
  set(ICU_PREFIX "icu4c-${ICU_VERSION}")
  set(ICU_72.1_SHA256
      "a2d2d38217092a7ed56635e34467f92f976b370e20182ad325edea6681a71d68")
  set(ICU_73.2_SHA256
      "818a80712ed3caacd9b652305e01afc7fa167e6f2e94996da44b90c2ab604ce1")
  string(REPLACE "." "-" ICU_VERSION_DASH ${ICU_VERSION})
  string(REPLACE "." "_" ICU_VERSION_UNDER ${ICU_VERSION})
  string(REGEX MATCH "^([0-9]+)" ICU_VERSION_MAJOR ${ICU_VERSION})
  set(ICU_VERSION_MAJOR
      ${ICU_VERSION_MAJOR}
      PARENT_SCOPE)

  #
  # Build native version
  #
  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  ExternalProject_Add(
    icu-native
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://github.com/unicode-org/icu/releases/download/release-${ICU_VERSION_DASH}/icu4c-${ICU_VERSION_UNDER}-src.tgz
    URL_HASH SHA256=${ICU_${ICU_VERSION}_SHA256}
    SOURCE_SUBDIR source
    CONFIGURE_COMMAND
      <SOURCE_DIR>/source/configure --prefix=${LIBS_INSTALL_PREFIX}
      --srcdir=<SOURCE_DIR>/source
    BUILD_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    INSTALL_COMMAND ""
    DEPENDS ${after_libs})
  ExternalProject_Get_Property(icu-native BINARY_DIR)
  set(icu-native-BINARY_DIR ${BINARY_DIR})
  add_dependencies(external_libs icu-native)

  #
  # Build cross-compiled version.
  #
  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  ExternalProject_Add(
    icu
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://github.com/unicode-org/icu/releases/download/release-${ICU_VERSION_DASH}/icu4c-${ICU_VERSION_UNDER}-src.tgz
    URL_HASH SHA256=${ICU_${ICU_VERSION}_SHA256}
    SOURCE_SUBDIR source
    PATCH_COMMAND patch -p1 < ${PROJECT_SOURCE_DIR}/patches/${ICU_PREFIX}.patch
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_CONFIGURE_ENV}
      <SOURCE_DIR>/source/configure ${PLATFORM_BUILD_AND_HOST}
      --prefix=${LIBS_INSTALL_PREFIX} --enable-shared=yes --enable-static=no
      --disable-extras --disable-tools --disable-tests --disable-samples
      --with-data-packaging=static --with-cross-build=${icu-native-BINARY_DIR}
      --srcdir=<SOURCE_DIR>/source
    BUILD_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    DEPENDS icu-native ${after_libs})
  add_dependencies(external_libs icu)
  message(STATUS "Will build icu (${ICU_VERSION}) ${PREBUILT_TAG}")
endfunction()
