#
# Copyright (C) 2022-2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Maybe build exiv2.  The exiv2 sources have been updated to support the C++17
# standard, but a C++17 version containing this support hasn't been released
# yet. Until it is, we download and build a recent enough version of exiv2.  The
# tag specified here is the tag used when the exiv2 code was copied into
# mythtv/external.
#
# A comment from 2023-01-18 in a github Exiv issue #2406 discussing releases:
#
# Dear folks, Exiv2 v0.27.6 has been released!  I'll start working on the v1.0.0
# major release based on 9ca161d.
#
# However, Exiv2 v0.28 was released with C++17 support included, so we can use
# that version if the distro includes it.
#
function(find_or_build_exiv2)
  if(LIBS_USE_INSTALLED)
    pkg_check_modules(EXIV2 "exiv2>=0.28" IMPORTED_TARGET)
    if(NOT EXIV2_FOUND)
      pkg_check_modules(EXIV2 "mythexiv2>=0.28" IMPORTED_TARGET)
    endif()
    if(TARGET PkgConfig::EXIV2)
      message(STATUS "Found Exiv2 ${EXIV2_VERSION} ${EXIV2_LINK_LIBRARIES}")
      set(ProjectDepends
          ${ProjectDepends} PkgConfig::EXIV2
          PARENT_SCOPE)
      return()
    endif()
  endif()

  if(CMAKE_CROSSCOMPILING)
    set(BUILD_XMP OFF)
  else()
    set(BUILD_XMP ON)
  endif()
  if(LIBS_INSTALL_EXIV2)
    set(CMDLINE_ARGS_EXIV2 ${CMDLINE_ARGS_LIBS})
  else()
    set(CMDLINE_ARGS_EXIV2 ${CMDLINE_ARGS})
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET embedded_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  if(CMAKE_CROSSCOMPILING OR ENABLE_EXIV2_DOWNLOAD)
    # Mon Nov 27 18:19:46 2023 +0000
    set(EXIV2_VERSION 2a5587785f26f9fc8e9484e73ccfb2762741c137)
    # ~~~
    # Rolling version.
    # set(EXIV2_VERSION "1.0.0.9")
    # ~~~

    set(EXIV2_PREFIX "exiv2-${EXIV2_VERSION}")
    set(EXIV2_2a5587785f26f9fc8e9484e73ccfb2762741c137_SHA256
        "36d1a386ad923f9d7406fcc29e9598f82c7744ad3fb70c862960a0cbd132b0a4")
    # For the 2023-10-04 download of nightly (aka 1.0.0.9)
    set(EXIV2_1.0.0.9_SHA256
        "509b7c3ad2df2b8e8a76bbf68fa24241012fde246c6a9e75529c75ee433ad142")

    if(NOT EXIV2_VERSION MATCHES "\\.")
      set(DOWNLOAD_URL
          https://github.com/Exiv2/exiv2/archive/${EXIV2_VERSION}.tar.gz)
    else()
      set(DOWNLOAD_URL
          https://github.com/Exiv2/exiv2/archive/refs/tags/nightly.tar.gz)
    endif()
    set(BUILD_INSTRUCTIONS
        DOWNLOAD_DIR
        ${TARBALL_DIR}
        URL
        ${DOWNLOAD_URL}
        DOWNLOAD_NAME
        exiv2-${EXIV2_VERSION}.tar.gz
        URL_HASH
        SHA256=${EXIV2_${EXIV2_VERSION}_SHA256}
        PATCH_COMMAND
        patch
        -p1
        -N
        -i
        ${PROJECT_SOURCE_DIR}/patches/${EXIV2_PREFIX}.patch)
  else(CMAKE_CROSSCOMPILING OR ENABLE_EXIV2_DOWNLOAD)
    set(EXIV2_VERSION embedded)
    set(BUILD_INSTRUCTIONS SOURCE_DIR
                           ${CMAKE_CURRENT_SOURCE_DIR}/mythtv/external/libexiv2)
  endif(CMAKE_CROSSCOMPILING OR ENABLE_EXIV2_DOWNLOAD)

  ExternalProject_Add(
    exiv2
    ${BUILD_INSTRUCTIONS}
    CMAKE_ARGS --no-warn-unused-cli
               -DCMAKE_INSTALL_SO_NO_EXE=OFF
               -DBUILD_SHARED_LIBS:BOOL=ON
               -DEXIV2_ENABLE_XMP:BOOL=${BUILD_XMP}
               -DEXIV2_ENABLE_BMFF:BOOL=OFF # HEIC, HEIF, AVIF, CR3, JXL/bmff
               -DEXIV2_ENABLE_BROTLI:BOOL=OFF # Google algorithm
               -DEXIV2_ENABLE_INIH=OFF
               -DEXIV2_BUILD_SAMPLES:BOOL=OFF
               -DEXIV2_BUILD_EXIV2_COMMAND:BOOL=OFF
               ${CMDLINE_ARGS_EXIV2}
               ${EXIV2_PLATFORM_ARGS}
               ${PLATFORM_ARGS}
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:PATH=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:PATH=${PKG_CONFIG_PATH}
    BUILD_ALWAYS ${LIBS_ALWAYS_REBUILD}
    USES_TERMINAL_BUILD TRUE
    DEPENDS external_libs ${after_libs})

  add_dependencies(embedded_libs exiv2)

  message(STATUS "Will build exiv2 (${EXIV2_VERSION})")
endfunction()

find_or_build_exiv2()
