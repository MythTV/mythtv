#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Perform windows specific customization.
#
if(NOT CMAKE_SYSTEM_NAME MATCHES "Windows" OR NOT CMAKE_CROSSCOMPILING)
  return()
endif()

#
# Validate variables
#
if(NOT DEFINED CMAKE_INSTALL_PREFIX OR CMAKE_INSTALL_PREFIX STREQUAL "")
  message(FATAL_ERROR "CMAKE_INSTALL_PREFIX isn't set.")
endif()
if(NOT DEFINED CMAKE_INSTALL_FULL_INCLUDEDIR OR CMAKE_INSTALL_FULL_INCLUDEDIR
                                                STREQUAL "")
  message(FATAL_ERROR "CMAKE_INSTALL_FULL_INCLUDEDIR isn't set.")
endif()
if(NOT DEFINED CMAKE_INSTALL_FULL_BINDIR OR CMAKE_INSTALL_FULL_BINDIR STREQUAL
                                            "")
  message(FATAL_ERROR "CMAKE_INSTALL_FULL_BINDIR isn't set.")
endif()

#
# Set PKG_CONFIG_LIBDOR so pkg_config lookups don't find any build system files.
#
set(PKG_CONFIG_LIBDIR ${MINGW_SYSROOT}/lib/pkgconfig)

#
# Set variables for later use when building with meson.
#
if(EXISTS ${PROJECT_SOURCE_DIR}/cmake/files/meson-cross-windows.in)
  configure_file(${PROJECT_SOURCE_DIR}/cmake/files/meson-cross-windows.in
                 ${PROJECT_BINARY_DIR}/meson-cross-compile @ONLY)
endif()

#
# Set general variables for later use when building libraries from source using
# configure/make.
#
set(PLATFORM_COMPILE_ENV_CMDS
    AR=${CMAKE_AR}
    CC=${CMAKE_C_COMPILER}
    CXX=${CMAKE_CXX_COMPILER}
    DLLTOOL=${CMAKE_DLLTOOL}
    NM=${CMAKE_NM}
    OBJDUMP=${CMAKE_OBJDUMP}
    RANLIB=${CMAKE_RANLIB}
    STRIP=${CMAKE_STRIP}
    WINDRES=${CMAKE_RC_COMPILER})
list(JOIN PKG_CONFIG_PATH ":" PKG_CONFIG_PATH_STR)
set(PLATFORM_COMPILE_ENV_FLAGS
    "CFLAGS=${CMAKE_C_FLAGS} -I${MINGW_SYSROOT}/include"
    "CXXFLAGS=${CMAKE_CXX_FLAGS} -I${MINGW_SYSROOT}/include"
    LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS}
    LIBS=-lssp
    PKG_CONFIG_LIBDIR=${PKG_CONFIG_LIBDIR}
    "PKG_CONFIG_PATH=${PKG_CONFIG_PATH_STR}")
set(PLATFORM_CONFIGURE_ENV ${PLATFORM_COMPILE_ENV_CMDS}
                           ${PLATFORM_COMPILE_ENV_FLAGS})
set(PLATFORM_MESON_ENV PKG_CONFIG=${PKG_CONFIG_EXECUTABLE}
                       ${PLATFORM_COMPILE_ENV_FLAGS})
set(PLATFORM_BUILD_AND_HOST "--build=${CMAKE_HOST_SYSTEM_PROCESSOR}"
                            "--host=${TOOLCHAIN_PREFIX} ")

# Libbluray needs extra includes
set(LIBBLURAY_COMPILE_ENV_FLAGS
    "CFLAGS=${CMAKE_C_FLAGS} -I${MINGW_SYSROOT}/include -I${CMAKE_INSTALL_FULL_INCLUDEDIR} -I${LIBS_INSTALL_PREFIX}/include"
    LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS}
    LIBS=-lssp
    PKG_CONFIG_LIBDIR=${PKG_CONFIG_LIBDIR}
    "PKG_CONFIG_PATH=${PKG_CONFIG_PATH_STR}")
set(LIBBLURAY_CONFIGURE_ENV ${PLATFORM_COMPILE_ENV_CMDS}
                            ${LIBBLURAY_COMPILE_ENV_FLAGS})

#
# Set variables for later use when building FFmpeg.
#
set(FF_PLATFORM_ARGS "--enable-cross-compile" "--ar=${CMAKE_AR}"
                     "--cc=${CMAKE_C_COMPILER}" "--cxx=${CMAKE_CXX_COMPILER}")
if(TOOLCHAIN_PREFIX STREQUAL "x86_64-w64-mingw32")
  list(APPEND FF_PLATFORM_ARGS "--target_os=mingw64")
else()
  list(APPEND FF_PLATFORM_ARGS "--target_os=mingw32")
endif()

#
# Set variables for later use when building OpenSSL.
#
set(OPENSSL_PLATFORM_CONFIG_ARGS --cross-compile-prefix=x86_64-w64-mingw32-
                                 mingw64 -lws2_32 -lssp)

#
# Set variables for later use when building Qt.
#
set(QT_PLATFORM_CONFIGURE_ENV ${PLATFORM_CONFIGURE_ENV}
                              "PKG_CONFIG_SYSROOT_DIR=${MINGW_SYSROOT}")

set(QT5_PLATFORM_ARGS
    # cmake-format: off
    -xplatform win32-g++
    -device-option CROSS_COMPILE=/usr/bin/x86_64-w64-mingw32-
    -no-avx
    -no-eventfd
    -no-feature-alloca_h
    -no-inotify
    -opengl dynamic
    -skip qtdeclarative
    # cmake-format: on
)
set(QT5_PLATFORM_LIBS_ARGS -I${MINGW_SYSROOT}/include
                           ZSTD_INCDIR=${MINGW_SYSROOT}/include/zstd)

set(QT6_PLATFORM_ARGS
    -DQT_QMAKE_TARGET_MKSPEC=win32-g++
    # -DQT_QMAKE_DEVICE_OPTIONS=CROSS_COMPILE=/usr/bin/x86_64-w64-mingw32-
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})

if(TOOLCHAIN_PREFIX MATCHES "mingw")
  # From settings.pro
  add_compile_definitions(WIN32 USING_MINGW WIN32_LEAN_AND_MEAN NOMINMAX)

  # Fix: redeclared without dllimport attribute after being referenced with dll
  # linkage
  add_compile_options(-Wno-attributes)

  # Windows puts .dll file in the bin directory, and .dll.a files in the lib
  # direcory.
  link_directories(${CMAKE_INSTALL_FULL_BINDIR})
  if(NOT LIBS_INSTALL_PREFIX STREQUAL CMAKE_INSTALL_PREFIX)
    link_directories(${LIBS_INSTALL_PREFIX}/bin)
  endif()
endif()
