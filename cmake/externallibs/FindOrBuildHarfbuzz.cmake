#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Harfbuzz
#
# meson, C++ source only
#
# The CMakeLists.txt provided by harfbuzz states that meson is the preferred
# build system and that cmake support might go away.
#
# Required by libass >= 0.15. Optional for freetype and fontconfig
#
function(find_or_build_harfbuzz)
  pkg_check_modules(HARFBUZZ "harfbuzz" QUIET IMPORTED_TARGET)
  if(HARFBUZZ_FOUND)
    message(
      STATUS "Found harfbuzz ${HARFBUZZ_VERSION} at ${HARFBUZZ_LINK_LIBRARIES}")
    add_library(harfbuzz ALIAS PkgConfig::HARFBUZZ)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(HARFBUZZ_VERSION "8.1.1")
  set(HARFBUZZ_PREFIX "harfbuzz-${HARFBUZZ_VERSION}")
  set(HARFBUZZ_6.0.0_SHA256
      "1d1010a1751d076d5291e433c138502a794d679a7498d1268ee21e2d4a140eb4")
  set(HARFBUZZ_8.1.1_SHA256
      "0305ad702e11906a5fc0c1ba11c270b7f64a8f5390d676aacfd71db129d6565f")
  ExternalProject_Add(
    harfbuzz
    DOWNLOAD_DIR ${TARBALL_DIR}
    # DOWNLOAD_EXTRACT_TIMESTAMP ON
    URL https://github.com/harfbuzz/harfbuzz/releases/download/${HARFBUZZ_VERSION}/${HARFBUZZ_PREFIX}.tar.xz
    URL_HASH SHA256=${HARFBUZZ_${HARFBUZZ_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 <
                  ${PROJECT_SOURCE_DIR}/patches/${HARFBUZZ_PREFIX}.patch
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_MESON_ENV} ${MESON_EXECUTABLE} setup
      --cross-file=${PROJECT_BINARY_DIR}/meson-cross-compile
      --buildtype=$<IF:$<CONFIG:Debug>,debug,release>
      --prefix=${LIBS_INSTALL_PREFIX} <BINARY_DIR> <SOURCE_DIR> -Ddocs=disabled
      -Dtests=disabled -Dfreetype=enabled
    BUILD_COMMAND ${MESON_EXECUTABLE} compile
    INSTALL_COMMAND ${MESON_EXECUTABLE} install
    DEPENDS freetype icu ${after_libs})

  add_dependencies(external_libs harfbuzz)

  message(STATUS "Will build harfbuzz (${HARFBUZZ_VERSION}) ${PREBUILT_TAG}")
endfunction()
