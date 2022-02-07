#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# lame encoder
#
# configure, C and C++ source
#
function(find_or_build_lame)
  find_package(Lame 3.98.3 MODULE QUIET)
  if(TARGET Lame::Lame)
    if(Lame_VERSION STREQUAL "unknown" OR Lame_VERSION VERSION_GREATER_EQUAL
                                          3.98.3)
      set(Lame_VERSION_OK ON)
    endif()
  endif()
  if(TARGET Lame::Lame AND Lame_VERSION_OK)
    message(STATUS "Found lame ${Lame_VERSION} at ${Lame_LIBRARIES}")
    if(Lame_VERSION STREQUAL "unknown")
      message(STATUS "Warning: Lame version unknown (requested 3.98.3)")
      message(STATUS "This is expected when cross-compiling lame.")
    endif()
    # add_build_config(Lame::Lame "libmp3lame")
    target_compile_definitions(Lame::Lame INTERFACE USING_LIBMP3LAME)
    add_library(lame ALIAS Lame::Lame)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(LAME_VERSION "3.100")
  string(REGEX MATCH "^([0-9]+\.[0-9]+)" LAME_MAJMIN ${LAME_VERSION})
  set(LAME_PREFIX "lame-${LAME_VERSION}")
  set(LAME_3.100_SHA256
      "ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e")
  ExternalProject_Add(
    lame
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://sourceforge.net/projects/lame/files/lame/${LAME_MAJMIN}/${LAME_PREFIX}.tar.gz
    URL_HASH SHA256=${LAME_${LAME_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 < ${PROJECT_SOURCE_DIR}/patches/${LAME_PREFIX}.patch
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_CONFIGURE_ENV} <SOURCE_DIR>/configure
      ${PLATFORM_BUILD_AND_HOST} ${CONFIGURE_VERBOSE_ARG}
      --prefix=${LIBS_INSTALL_PREFIX} --enable-shared=yes --enable-static=no
      --srcdir=<SOURCE_DIR> --disable-gtktest --disable-frontend
    BUILD_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    DEPENDS ${after_libs})

  add_dependencies(external_libs lame)

  message(STATUS "Will build lame (${LAME_VERSION})")
endfunction()
