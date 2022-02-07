#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# fftw
#
# cmake, C source
#
# fftw must be built twice.  Once with floating point support, once without.
#
function(find_or_build_fftw)
  set(FFTW_VERSION "3.3.10")
  set(FFTW_PREFIX "fftw-${FFTW_VERSION}")
  set(FFTW_3.3.8_SHA256
      "6113262f6e92c5bd474f2875fa1b01054c4ad5040f6b0da7c03c98821d9ae303")
  set(FFTW_3.3.10_SHA256
      "56c932549852cddcfafdab3820b0200c7742675be92179e59e6215b340e26467")

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  pkg_check_modules(FFTW3 "fftw3" QUIET IMPORTED_TARGET)
  if(FFTW3_FOUND)
    message(STATUS "Found fftw ${FFTW_VERSION} at ${FFTW3_LINK_LIBRARIES}")
    add_library(fftw ALIAS PkgConfig::FFTW3)
  else()
    ExternalProject_Add(
      fftw
      DOWNLOAD_DIR ${TARBALL_DIR}
      URL http://www.fftw.org/${FFTW_PREFIX}.tar.gz
      URL_HASH SHA256=${FFTW_${FFTW_VERSION}_SHA256}
      PATCH_COMMAND patch -p1 <
                    ${PROJECT_SOURCE_DIR}/patches/${FFTW_PREFIX}.patch
      CMAKE_ARGS --no-warn-unused-cli
                 ${CMDLINE_ARGS_LIBS}
                 ${PLATFORM_ARGS}
                 -DBUILD_SHARED_LIBS:BOOL=ON
                 -DBUILD_TESTS:BOOL=OFF
                 -DENABLE_THREADS:BOOL=ON
                 -DENABLE_FLOAT:BOOL=OFF
                 -DDISABLE_FORTRAN:BOOL=ON
      CMAKE_CACHE_ARGS
        -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
        -DCMAKE_JOB_POOL_COMPILE:STRING=compile
        -DCMAKE_JOB_POOL_LINK:STRING=link
        -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
        -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
        -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
      DEPENDS ${after_libs})

    add_dependencies(external_libs fftw)

    message(STATUS "Will build fftw (${FFTW_VERSION}) ${PREBUILT_TAG}")
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  pkg_check_modules(FFTW3F "fftw3f" QUIET IMPORTED_TARGET)
  if(FFTW3F_FOUND)
    message(STATUS "Found fftwf ${FFTW3F_VERSION} at ${FFTW3F_LINK_LIBRARIES}")
    add_library(fftwf ALIAS PkgConfig::FFTW3F)
  else()
    ExternalProject_Add(
      fftwf
      DOWNLOAD_DIR ${TARBALL_DIR}
      URL http://www.fftw.org/${FFTW_PREFIX}.tar.gz
      URL_HASH SHA256=${FFTW_${FFTW_VERSION}_SHA256}
      PATCH_COMMAND patch -p1 <
                    ${PROJECT_SOURCE_DIR}/patches/${FFTW_PREFIX}.patch
      CMAKE_ARGS --no-warn-unused-cli
                 ${CMDLINE_ARGS_LIBS}
                 ${PLATFORM_ARGS}
                 -DBUILD_SHARED_LIBS:BOOL=ON
                 -DBUILD_TESTS:BOOL=OFF
                 -DENABLE_THREADS:BOOL=ON
                 -DENABLE_FLOAT:BOOL=ON
                 -DDISABLE_FORTRAN:BOOL=ON
      CMAKE_CACHE_ARGS
        -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
        -DCMAKE_JOB_POOL_COMPILE:STRING=compile
        -DCMAKE_JOB_POOL_LINK:STRING=link
        -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
        -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
        -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
      DEPENDS ${after_libs})

    add_dependencies(external_libs fftwf)

    message(STATUS "Will build fftwf (${FFTW_VERSION}) ${PREBUILT_TAG}")
  endif()

endfunction()
