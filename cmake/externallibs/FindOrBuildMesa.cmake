#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# mesa
#
# meson, C source
#
function(find_or_build_mesa)
  pkg_check_modules(MESA "glesv2" QUIET IMPORTED_TARGET)
  if(MESA_FOUND)
    message(STATUS "Found mesa ${MESA_VERSION} at ${MESA_LINK_LIBRARIES}")
    add_library(mesa ALIAS PkgConfig::MESA)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(MESA_VERSION "23.1.6")
  set(MESA_PREFIX "mesa-${MESA_VERSION}")
  set(MESA_22.2.5_SHA256
      "042e6b18d778c88fc07c4b50c5aa649eb99bf7bd83adb6a91ccd8327546a3ae0")
  set(MESA_22.3.5_SHA256
      "d65b9efbdc5a478e10ea08599740eec89f95b9b85c64b68fe9afa8bacef5c28f")
  set(MESA_23.1.6_SHA256
      "8a124bd8ea3b1adc1d57adcb1fbb9aa81103060e1e3b56f38b43514a12179bf0")
  ExternalProject_Add(
    mesa
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://gitlab.freedesktop.org/mesa/mesa/-/archive/${MESA_PREFIX}/mesa-${MESA_PREFIX}.tar.bz2
    URL_HASH SHA256=${MESA_${MESA_VERSION}_SHA256}
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_MESON_ENV}
      PKG_CONFIG=${PKG_CONFIG_EXECUTABLE} ${MESON_EXECUTABLE} setup
      --cross-file=${PROJECT_BINARY_DIR}/meson-cross-compile
      --prefix=${LIBS_INSTALL_PREFIX} -Dbuild-tests=false -Dplatforms=windows
      -Degl-native-platform=windows
      -Dbuildtype=$<IF:$<CONFIG:Debug>,debug,release> -Dshared-glapi=enabled
      -Dgles1=enabled -Dgles2=enabled -Degl=enabled -Dgallium-drivers=d3d12
      # -Dgallium-drivers=d3d12,swrast,iris,zink -Dvulkan-drivers=intel,amd
      -Dosmesa=false <BINARY_DIR> <SOURCE_DIR>
    BUILD_COMMAND ${MESON_EXECUTABLE} compile
    INSTALL_COMMAND ${MESON_EXECUTABLE} install
    DEPENDS zlib ${after_libs})

  add_dependencies(external_libs mesa)

  message(STATUS "Will build mesa (${MESA_VERSION})")
endfunction()
