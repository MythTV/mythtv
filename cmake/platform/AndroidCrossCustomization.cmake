#
# Copyright (C) 2022-2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Perform android specific customization. This must come after the project
# command, as that's where the android ndk cmake files are loaded.
#
if(NOT ANDROID OR NOT CMAKE_CROSSCOMPILING)
  return()
endif()

#
# Validate variables
#
if(NOT DEFINED CMAKE_INSTALL_PREFIX OR CMAKE_INSTALL_PREFIX STREQUAL "")
  message(FATAL_ERROR "CMAKE_INSTALL_PREFIX isn't set.")
endif()
if(NOT ANDROID_PLATFORM)
  if(CMAKE_SYSTEM_VERSION GREATER 34)
    message(
      FATAL_ERROR
        "Cannot build to version ${CMAKE_SYSTEM_VERSION}.  ANDROID_PLATFORM higher than 34 is not currently supported."
    )
  endif()
  set(ANDROID_PLATFORM android-${CMAKE_SYSTEM_VERSION})
else()
  string(REPLACE "android-" "" PLATFORM_NUM ${ANDROID_PLATFORM})
  if(PLATFORM_NUM GREATER 34)
    message(
      FATAL_ERROR "ANDROID_PLATFORM higher than 34 is not currently supported.")
  endif()
endif()

# Set up the PLATFORM_ARGS variable to contain arguments to pass to any cmake
# based external projects.
list(
  APPEND
  PLATFORM_ARGS
  "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
  "-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}"
  "-DCMAKE_ANDROID_NDK=${CMAKE_ANDROID_NDK}"
  "-DCMAKE_ANDROID_ARCH_ABI=${CMAKE_ANDROID_ARCH_ABI}")

# Fix the toolchain prefix variables. The android ndk screws them up for 32-bit
# builds. No, really! It does.
if(CMAKE_ANDROID_ARCH STREQUAL "arm")
  set(CMAKE_C_ANDROID_TOOLCHAIN_MACHINE "armv7a-linux-androideabi")
  if(CMAKE_C_ANDROID_TOOLCHAIN_PREFIX)
    string(
      REGEX
      REPLACE "arm-linux-androideabi" "armv7a-linux-androideabi"
              CMAKE_C_ANDROID_TOOLCHAIN_PREFIX
              ${CMAKE_C_ANDROID_TOOLCHAIN_PREFIX})
  endif()
  if(CMAKE_CXX_ANDROID_TOOLCHAIN_PREFIX)
    string(
      REGEX
      REPLACE "arm-linux-androideabi" "armv7a-linux-androideabi"
              CMAKE_CXX_ANDROID_TOOLCHAIN_PREFIX
              ${CMAKE_CXX_ANDROID_TOOLCHAIN_PREFIX})
  endif()
endif()

#
# Set PKG_CONFIG_LIBDIR so pkg_config lookups don't find any build system
# libraries.  This would normally get set to the sysroot pkgconfig directory.
# The android ndk sysroot doesn't have a pkgconfig directory, butpoint to where
# it should be anyway so no build system files sneak in.
#
set(PKG_CONFIG_LIBDIR ${CMAKE_ANDROID_NDK_TOOLCHAIN_UNIFIED}/usr/lib/pkgconfig)

#
# Set general variables for later use when building libraries from source using
# configure/make.
#
set(PLATFORM_COMPILE_ENV_CMDS
    AR=${CMAKE_AR}
    "CC=${CMAKE_C_COMPILER} --target=${CMAKE_C_COMPILER_TARGET}"
    "CXX=${CMAKE_CXX_COMPILER} --target=${CMAKE_C_COMPILER_TARGET}"
    NM=${CMAKE_NM}
    OBJDUMP=${CMAKE_OBJDUMP}
    RANLIB=${CMAKE_RANLIB}
    STRIP=${CMAKE_STRIP})
list(JOIN PKG_CONFIG_PATH ":" PKG_CONFIG_PATH_STR)
set(PLATFORM_COMPILE_ENV_FLAGS
    "CFLAGS=${CMAKE_C_FLAGS} --target=${CMAKE_C_COMPILER_TARGET}"
    "CXXFLAGS=${CMAKE_CXX_FLAGS} --target=${CMAKE_CXX_COMPILER_TARGET}"
    "LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS} --target=${CMAKE_C_COMPILER_TARGET}"
    PKG_CONFIG_LIBDIR=${PKG_CONFIG_LIBDIR}
    "PKG_CONFIG_PATH=${PKG_CONFIG_PATH_STR}")
set(PLATFORM_CONFIGURE_ENV ${PLATFORM_COMPILE_ENV_CMDS}
                           ${PLATFORM_COMPILE_ENV_FLAGS})
set(PLATFORM_MESON_ENV PKG_CONFIG=${PKG_CONFIG_EXECUTABLE}
                       ${PLATFORM_COMPILE_ENV_FLAGS})
set(PLATFORM_BUILD_AND_HOST "--build=${CMAKE_HOST_SYSTEM_PROCESSOR}"
                            "--host=${CMAKE_C_COMPILER_TARGET} ")

# Libbluray needs extra includes on other platforms
set(LIBBLURAY_CONFIGURE_ENV ${PLATFORM_CONFIGURE_ENV})

#
# Set variables for later use when building with meson.
#
if(CMAKE_C_ANDROID_TOOLCHAIN_PREFIX)
  string(REGEX REPLACE "-$" "${CMAKE_SYSTEM_VERSION}-clang" CROSS_CC
                       ${CMAKE_C_ANDROID_TOOLCHAIN_PREFIX})
endif()
if(CMAKE_CXX_ANDROID_TOOLCHAIN_PREFIX)
  string(REGEX REPLACE "-$" "${CMAKE_SYSTEM_VERSION}-clang++" CROSS_CXX
                       ${CMAKE_CXX_ANDROID_TOOLCHAIN_PREFIX})
endif()
if(EXISTS ${PROJECT_SOURCE_DIR}/cmake/files/meson-cross-android.in)
  if(CMAKE_ANDROID_ARCH STREQUAL "arm64")
    set(MESON_SYSTEM_CPU_FAMILY "aarch64")
  else()
    set(MESON_SYSTEM_CPU_FAMILY ${CMAKE_ANDROID_ARCH})
  endif()
  configure_file(${PROJECT_SOURCE_DIR}/cmake/files/meson-cross-android.in
                 ${PROJECT_BINARY_DIR}/meson-cross-compile @ONLY)
  unset(MESON_SYSTEM_CPU_FAMILY)
endif()

#
# Set variables for later use when building FFmpeg.
#
set(FF_PLATFORM_ARGS
    "--enable-cross-compile" "--target_os=android"
    "--arch=${CMAKE_ANDROID_ARCH}" "--cc=${CROSS_CC}" "--cxx=${CROSS_CXX}")
if(${CMAKE_ANDROID_ARCH} STREQUAL "x86")
  # Fix "relocation R_386_32 cannot be used against..." warnings.
  list(APPEND FF_PLATFORM_ARGS "--disable-asm")
endif()

#
# Set variables for later use when building OpenSSL.
#
set(OPENSSL_PLATFORM_CONFIG_ENV
    ANDROID_NDK_ROOT=${CMAKE_ANDROID_NDK}
    ANDROID_SDK=${CMAKE_SYSTEM_VERSION}
    "PATH=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin:$ENV{PATH}"
)
set(OPENSSL_PLATFORM_CONFIG_ARGS android-${CMAKE_ANDROID_ARCH})
set(OPENSSL_PLATFORM_BUILD_ARGS
    "PATH=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin:$ENV{PATH}"
)

#
# Set variables for later use when building Qt.  This variable is also required
# when using the find_package() function to look up how to include/link to the
# Qt package.  Its referenced in one of the code snippets provided by the Qt
# build.
#
set(ANDROID_ABI ${CMAKE_ANDROID_ARCH_ABI})

#
# Set variables for later use when building Qt.
#
set(QT_PLATFORM_CONFIGURE_ENV
    ${PLATFORM_CONFIGURE_ENV} "PATH=${JAVA_HOME}/bin:$ENV{PATH}"
    "PKG_CONFIG_SYSROOT_DIR=${CMAKE_ANDROID_NDK_TOOLCHAIN_UNIFIED}")
set(QT_PLATFORM_BUILD_ENV ${PLATFORM_BUILD_ENV}
                          "PATH=${JAVA_HOME}/bin:$ENV{PATH}")
set(QT_PLATFORM_INSTALL_ENV ${PLATFORM_INSTALL_ENV}
                            "PATH=${JAVA_HOME}/bin:$ENV{PATH}")

set(QT5_PLATFORM_ARGS
    # cmake-format: off
    -android-arch ${CMAKE_ANDROID_ARCH_ABI}
    -android-ndk ${CMAKE_ANDROID_NDK}
    -android-ndk-platform android-${CMAKE_SYSTEM_VERSION}
    -xplatform android-clang
    # cmake-format: on
)
set(QT5_PLATFORM_LIBS_ARGS "ICU_LIBS=-licui18n -licuuc -licudata")
set(QT6_PLATFORM_ARGS
    -DANDROID_ABI=${CMAKE_ANDROID_ARCH_ABI}
    -DANDROID_NDK=${CMAKE_ANDROID_NDK}
    -DANDROID_SDK_ROOT=${ANDROID_SDK}
    -DCMAKE_SYSTEM_NAME=Android
    -DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake
)

if(ANDROID_MIN_SDK_VERSION LESS 28)
  # Fix compiling of libxml2.  Android < 28 doesn't include iconv, so we have to
  # build it ourselves.  Make sure the compile of XML2 finds the include file
  # from our build, instead of from the Android SDK.   Iconv vs libiconv is a
  # PITA.
  list(APPEND EXIV2_PLATFORM_ARGS
       -DIconv_INCLUDE_DIR=${LIBS_INSTALL_PREFIX}/include)
  list(APPEND XML2_PLATFORM_ARGS
       -DIconv_INCLUDE_DIR=${LIBS_INSTALL_PREFIX}/include)
endif()

# Sigh, Qt cmake files don't use the same variable names as cmake proper uses.
# Set up another variable to handle the mapping when building things that use
# qt.
set(USES_QT_PLATFORM_ARGS "-DANDROID_ABI=${CMAKE_ANDROID_ARCH_ABI}")
if(${QT_VERSION_MAJOR} EQUAL 5)
  list(
    APPEND
    USES_QT_PLATFORM_ARGS
    "-DANDROID_HOST_TAG=${CMAKE_ANDROID_NDK_TOOLCHAIN_HOST_TAG}"
    "-DANDROID_NDK=${CMAKE_ANDROID_NDK}"
    "-DANDROID_TOOLCHAIN_ROOT=${CMAKE_ANDROID_NDK_TOOLCHAIN_UNIFIED}")
endif()

#
# Tell the mythtv proper build to not bother with the bindings.
#
set(ENABLE_BINDINGS_PYTHON OFF)
set(ENABLE_BINDINGS_PERL OFF)
set(ENABLE_BINDINGS_PHP OFF)
