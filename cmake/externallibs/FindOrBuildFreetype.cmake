#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Freetype
#
# cmake, C source only
#
function(find_or_build_freetype)
  pkg_check_modules(FT2 "freetype2" QUIET IMPORTED_TARGET)
  if(FT2_FOUND)
    message(
      STATUS
        "Found freetype ${FT2_VERSION} at ${FT2_LIBRARY_DIRS}/libfreetype.so")
    add_library(freetype ALIAS PkgConfig::FT2)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(FREETYPE_VERSION "2.13.2")
  set(FREETYPE_PREFIX "freetype-${FREETYPE_VERSION}")
  set(FREETYPE_2.12.1_SHA256
      "4766f20157cc4cf0cd292f80bf917f92d1c439b243ac3018debf6b9140c41a7f")
  set(FREETYPE_2.13.2_SHA256
      "12991c4e55c506dd7f9b765933e62fd2be2e06d421505d7950a132e4f1bb484d")
  ExternalProject_Add(
    freetype
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL http://download.savannah.gnu.org/releases/freetype/${FREETYPE_PREFIX}.tar.xz
    URL_HASH SHA256=${FREETYPE_${FREETYPE_VERSION}_SHA256}
    CMAKE_ARGS --no-warn-unused-cli ${CMDLINE_ARGS_LIBS} ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON -DFT_WITH_ZLIB:BOOL=ON
               -DFT_DISABLE_HARFBUZZ:BOOL=ON
    CMAKE_CACHE_ARGS
      # Never build a "debug" configuration
      -DCMAKE_BUILD_TYPE:STRING=$<IF:$<CONFIG:Debug>,RelWithDebInfo,$<CONFIG>>
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ${after_libs})

  add_dependencies(external_libs freetype)

  message(STATUS "Will build freetype (${FREETYPE_VERSION}) ${PREBUILT_TAG}")
endfunction()
