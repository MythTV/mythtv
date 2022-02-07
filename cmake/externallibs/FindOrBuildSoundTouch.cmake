#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# soundtouch
#
# cmake, C++ source
#
function(find_or_build_soundtouch)
  pkg_check_modules(SOUNDTOUCH "soundtouch" QUIET IMPORTED_TARGET)
  if(SOUNDTOUCH_FOUND)
    message(
      STATUS
        "Found soundtouch ${SOUNDTOUCH_VERSION} at ${SOUNDTOUCH_LINK_LIBRARIES}"
    )
    add_library(soundtouch ALIAS PkgConfig::SOUNDTOUCH)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(SOUNDTOUCH_VERSION "2.3.2")
  set(SOUNDTOUCH_PREFIX "soundtouch-${SOUNDTOUCH_VERSION}")
  set(SOUNDTOUCH_2.3.1_SHA256
      "6900996607258496ce126924a19fe9d598af9d892cf3f33d1e4daaa9b42ae0b1")
  set(SOUNDTOUCH_2.3.2_SHA256
      "3bde8ddbbc3661f04e151f72cf21ca9d8f8c88e265833b65935b8962d12d6b08")
  ExternalProject_Add(
    soundtouch
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://www.surina.net/soundtouch/${SOUNDTOUCH_PREFIX}.tar.gz
    URL_HASH SHA256=${SOUNDTOUCH_${SOUNDTOUCH_VERSION}_SHA256}
    CMAKE_ARGS --no-warn-unused-cli ${CMDLINE_ARGS_LIBS} ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ${after_libs})

  add_dependencies(external_libs soundtouch)

  message(STATUS "Will build soundtouch (${SOUNDTOUCH_VERSION})")
endfunction()
