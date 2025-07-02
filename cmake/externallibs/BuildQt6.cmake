#
# Copyright (C) 2022-2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Qt
#
# cmake, C and C++ source
#
function(find_or_build_qt)
  #
  # Limited check for Qt. Just the core module.
  #
  find_package(Qt6 ${QT6_MIN_VERSION_STR} QUIET NO_MODULE COMPONENTS Core)
  if(TARGET Qt6::Core)
    if("${Qt6_DIR}" MATCHES "(.*/qt)(/.*)")
      set(DIR ${CMAKE_MATCH_1})
    endif()
    message(STATUS "Found Qt6 ${Qt6Core_VERSION} in ${DIR}")
    add_library(Qt6 SHARED IMPORTED)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  # Qt6 cross compiling requires that a host installation be present to provide
  # all the tooling.  This vaiable tells cmake where to find all the Qt6 host
  # system bits.
  if(NOT QT6_HOST_PATH)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
      set(QT6_HOST_PATH /usr)
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD")
      set(QT6_HOST_PATH /usr/local)
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
      set(QT6_HOST_PATH /usr)
    else()
      message(
        WARNING
          "Don't know how to set QT6_HOST_PATH for a ${CMAKE_HOST_SYSTEM_NAME} system."
      )
    endif()
  endif()

  # Find where packages store their cmake modules.  This varies by distro.
  foreach(DIR ${QT6_HOST_PATH}/lib64/cmake ${QT6_HOST_PATH}/lib/cmake
              ${QT6_HOST_PATH}/lib/x86_64-linux-gnu/cmake)
    if(EXISTS ${DIR})
      set(QT6_HOST_PATH_CMAKE_DIR ${DIR})
      break()
    endif()
  endforeach()
  if(NOT EXISTS ${QT6_HOST_PATH_CMAKE_DIR})
    message(
      FATAL_ERROR
        "Cannot find the cmake install directory. Should be something like /usr/lib/cmake."
    )
  endif()

  # Check specific package availability. A user is likely to have the
  # qt6-qtbase-devel package installed, but far less likely to know they also
  # need the various other qt6 tools devel packages installed.
  if(NOT EXISTS ${QT6_HOST_PATH_CMAKE_DIR}/Qt6ShaderTools)
    message(
      "Directory ${QT6_HOST_PATH_CMAKE_DIR}/Qt6ShaderTools doesn't exist.")
    message(
      FATAL_ERROR "You must install the qt6 qtshadertools development package.")
  endif()
  if(NOT EXISTS ${QT6_HOST_PATH_CMAKE_DIR}/Qt6QmlTools)
    message("Directory ${QT6_HOST_PATH_CMAKE_DIR}/Qt6QmlTools doesn't exist.")
    message(
      FATAL_ERROR "You must install the qt6 qtdeclarative development package.")
  endif()

  # Known versions of Qt6 (as of 2025-003-11)

  # None: Debian 11, RHEL8, Ubuntu 20.04

  # Ubuntu 22.04
  set(QT_6.2.4_SHA256
      "cfe41905b6bde3712c65b102ea3d46fc80a44c9d1487669f14e4a6ee82ebb8fd")
  # Debian 12, Ubuntu 24.04
  set(QT_6.4.2_SHA256
      "689f53e6652da82fccf7c2ab58066787487339f28d1ec66a8765ad357f4976be")
  set(QT_6.5.2_SHA256
      "cde57be663d0f875759797298bdc37a936d517c39f2013e4e6ece5e12edeed12")
  # Fedora 38
  set(QT_6.6.0_SHA256
      "652538fcb5d175d8f8176c84c847b79177c87847b7273dccaec1897d80b50002")
  set(QT_6.6.1_SHA256
      "dd3668f65645fe270bc615d748bd4dc048bd17b9dc297025106e6ecc419ab95d")
  # Centos 9, Fedora 39, RHEL9, Ubuntu 24.10
  set(QT_6.6.2_SHA256
      "3c1e42b3073ade1f7adbf06863c01e2c59521b7cc2349df2f74ecd7ebfcb922d")
  # SuSe 15
  set(QT_6.6.3_SHA256
      "69d0348fef415da98aa890a34651e9cfb232f1bffcee289b7b4e21386bf36104")
  set(QT_6.7.2_SHA256
      "0aaea247db870193c260e8453ae692ca12abc1bd841faa1a6e6c99459968ca8a")
  # Centos 10
  set(QT_6.8.1_SHA256
      "45e3a9f6d33c92ffe65a1fde1a8eba5b228112df675f7f9026eaa332b2e2edff")
  # Arch, Debian Unstable, Fedora 40/41/42, Gentoo Suse Tumbleweed,
  # Ubuntu Rolling Rhino
  set(QT_6.8.2_SHA256
      "659d8bb5931afac9ed5d89a78e868e6bd00465a58ab566e2123db02d674be559")
  # Fedora Rawhide
  set(QT_6.9.0_SHA256
      "4f61e50551d0004a513fefbdb0a410595d94812a48600646fb7341ea0d17e1cb")

  # Qt6 requires that the version of the host tools match the version being
  # built.  What are the fallback target versions to build if there isn't an
  # exact version match.  The fallback must share the same major/minor version
  # number as the host version.
  set(QT_MAP_6.2 "6.2.4")
  set(QT_MAP_6.4 "6.4.2")
  set(QT_MAP_6.5 "6.5.2")
  set(QT_MAP_6.6 "6.6.1")
  set(QT_MAP_6.7 "6.7.2")
  set(QT_MAP_6.8 "6.8.1")
  set(QT_MAP_6.9 "6.9.0")

  # Grab the host version directly. so as not to pollute our cross-build setup.
  file(STRINGS
       "${QT6_HOST_PATH_CMAKE_DIR}/Qt6Core/Qt6CoreConfigVersionImpl.cmake"
       HOST_QT_STRING REGEX "set\\\(PACKAGE_VERSION \"[0-9\\\.]+\"\\\)")
  if("${HOST_QT_STRING}" MATCHES "([0-9]+\.[0-9]+)\.[0-9]+")
    set(HOST_QT_VERSION ${CMAKE_MATCH_0})
    set(HOST_QT_MAJMIN ${CMAKE_MATCH_1})
    set(QT_FALLBACK "${QT_MAP_${CMAKE_MATCH_1}}")
  endif()

  # Check for a complete version match first. If there isn't one, look for a
  # match of the major/minor versions.
  if(NOT "${QT_${HOST_QT_VERSION}_SHA256}" STREQUAL "")
    set(QT_VERSION "${HOST_QT_VERSION}")
  elseif(NOT "${QT_FALLBACK}" STREQUAL "" AND NOT "${QT_${QT_FALLBACK}_SHA256}"
                                              STREQUAL "")
    set(QT_VERSION "${QT_FALLBACK}")
  endif()
  if("${QT_VERSION}" STREQUAL "")
    message(
      FATAL_ERROR
        "Qt6 requires that the target and host major/minor version are the same.  Your host version is ${HOST_QT_MAJMIN} but there are no instructions on how to build that version."
    )
  endif()

  # Qt6 cross compiling requires that a native version of Qt6 be installed on
  # the build system.  The version being built for the target must be the same
  # as (or less??) than your installed version of Qt6.
  string(REGEX MATCH "^([0-9]+\.[0-9]+)" QT_MAJMIN ${QT_VERSION})
  set(QT_PREFIX "qt-${QT_VERSION}")
  set(QT_URL
      "https://download.qt.io/archive/qt/${QT_MAJMIN}/${QT_VERSION}/single/qt-everywhere-src-${QT_VERSION}.tar.xz"
  )

  # Get MariaDB information
  if(TARGET PkgConfig::MARIADB)
    get_target_property(MARIADB_INCLUDE_DIR PkgConfig::MARIADB
                        INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(MARIADB_LIBRARY PkgConfig::MARIADB
                        INTERFACE_LINK_LIBRARIES)
  elseif(WIN32)
    set(MARIADB_INCLUDE_DIR ${LIBS_INSTALL_PREFIX}/include/mariadb)
    set(MARIADB_LIBRARY ${LIBS_INSTALL_PREFIX}/lib/mariadb/libmariadb.dll.a)
  else()
    set(MARIADB_INCLUDE_DIR ${LIBS_INSTALL_PREFIX}/include/mariadb)
    set(MARIADB_LIBRARY ${LIBS_INSTALL_PREFIX}/lib/mariadb/libmariadb.so)
  endif()

  # See qtbase/cmake/configure-cmake-mapping.md for qmake to cmake argument
  # mappings.

  ExternalProject_Add(
    qt
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL ${QT_URL}
    URL_HASH SHA256=${QT_${QT_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 < ${PROJECT_SOURCE_DIR}/patches/${QT_PREFIX}.patch
    CMAKE_ARGS # cmake-format: off
       -G Ninja

       # Required platform specific arguments
       ${QT6_PLATFORM_ARGS}

       # Where is the installed host version of Qt6? The
       # QT_HOST_PATH_CMAKE_DIR define is needed because Fedora puts
       # the cmake files into lib64 instead of lib.
       -DQT_HOST_PATH=${QT6_HOST_PATH}
       -DQT_HOST_PATH_CMAKE_DIR=${QT6_HOST_PATH_CMAKE_DIR}

       # Where to install files
       -DCMAKE_INSTALL_PREFIX=${LIBS_INSTALL_PREFIX}/qt

       # Other android arguments
       -DBUILD_SHARED_LIBS=ON
       -DCMAKE_BUILD_TYPE=$<IF:$<CONFIG:Debug>,Debug,Release>
       -DQT_BUILD_EXAMPLES=OFF
       -DQT_BUILD_TESTS=OFF
       -DQT_USE_CCACHE=ON

       # Force Qt6 to use pkg-config for an android build.  It would
       # normally refuse because its building for android (or apple or
       # win32 or qnx or wasm).
       -DQT_FEATURE_pkg_config=ON
       -DQT_FEATURE_system_sqlite=OFF
       -DQT_FEATURE_openssl_linked=ON

       # Qt6 no longer allows disabling dpi scaling with the
       # AA_DisableHighDpiScaling attribute like it did in Qt5.  This
       # next line will prevent scaling support from being built for
       # Android, but this is *NOT* a long term solution.  Dpi scaling
       # is going to be a problem on any high dpi displays in Qt6.
       # Fixing MythTV to understand High DPI values should solve this
       # class of problems for all devices.
       -DQT_FEATURE_highdpiscaling=OFF

       # Qt6 components
       -DBUILD_qt3d=OFF
       -DBUILD_qt5compat=OFF
       -DBUILD_qtactiveqt=OFF
       -DBUILD_qtcharts=OFF
       -DBUILD_qtcoap=OFF
       -DBUILD_qtconnectivity=OFF
       -DBUILD_qtdatavis3d=OFF
       -DBUILD_qtdeclarative=OFF
       -DBUILD_qtdoc=OFF
       -DBUILD_qtgraphs=OFF
       -DBUILD_qthttpserver=OFF
#       -DBUILD_qtimageformats=OFF
       -DBUILD_qtlocation=OFF
       -DBUILD_qtlottie=OFF
       -DBUILD_qtmqtt=OFF
       -DBUILD_qtmultimedia=OFF
#       -DBUILD_qtnetworkauth=OFF
       -DBUILD_qtopcua=OFF
       -DBUILD_qtpositioning=OFF
       -DBUILD_qtquick3d=OFF
       -DBUILD_qtquick3dphysics=OFF
       -DBUILD_qtquickeffectmaker=OFF
       -DBUILD_qtquicktimeline=OFF
       -DBUILD_qtremoteobjects=OFF
       -DBUILD_qtscxml=OFF
       -DBUILD_qtsensors=OFF
       -DBUILD_qtserialbus=OFF
       -DBUILD_qtserialport=OFF
#       -DBUILD_qtshadertools=OFF
       -DBUILD_qtspeech=OFF
       -DBUILD_qtsvg=OFF
       -DBUILD_qttools=OFF
       -DBUILD_qttranslations=OFF
       -DBUILD_qtvirtualkeyboard=OFF
       -DBUILD_qtwayland=OFF
       -DBUILD_qtwebchannel=OFF
       -DBUILD_qtwebengine=OFF
       -DBUILD_qtwebsockets=OFF
       -DBUILD_qtwebview=OFF

       -DMySQL_INCLUDE_DIR=${MARIADB_INCLUDE_DIR}
       -DMySQL_LIBRARY=${MARIADB_LIBRARY}
       ${QT6_PLATFORM_LIBS_ARGS}
       # cmake-format: on
    #
    CMAKE_CACHE_ARGS
      # -DQT_EXTRA_DEFINES:STRING=ANDROID_PLATFORM=android-29;ANDROID_SDK_ROOT=/home/david/Android/Sdk;CMAKE_ANDROID_ARCH_ABI=arm64-v8a;ANDROID_NDK=/home/david/Android/Sdk/ndk/25.0.8775105;CMAKE_SYSTEM_NAME=Android;CMAKE_SYSTEM_VERSION=29;CMAKE_TOOLCHAIN_FILE=/home/david/Android/Sdk/ndk/25.0.8775105/build/cmake/android.toolchain.cmake
      -DQT_EXTRA_INCLUDEPATHS:STRING=/home/david/Projects/MythTV/cmake23/libs_install_arm64-v8a/include;/home/david/Projects/MythTV/cmake23/libs_install_arm64-v8a/include/mariadb
      -DQT_EXTRA_LIBDIRS:STRING=/home/david/Projects/MythTV/cmake23/libs_install_arm64-v8a/lib;/home/david/Projects/MythTV/cmake23/libs_install_arm64-v8a/lib64;/home/david/Projects/MythTV/cmake23/libs_install_arm64-v8a/lib/mariadb;/home/david/Projects/MythTV/cmake23/libs_install_arm64-v8a/lib64/mariadb
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DPKG_CONFIG_EXECUTABLE:STRING=${PKG_CONFIG_EXECUTABLE}
      # -DPKG_CONFIG_LIBDIR:PATH=${PKG_CONFIG_LIBDIR}
      # -DPKG_CONFIG_PATH:PATH=${PKG_CONFIG_PATH}
    USES_TERMINAL_CONFIGURE ON
    USES_TERMINAL_BUILD ON
    USES_TERMINAL_INSTALL ON
    DEPENDS ${after_libs})

  add_dependencies(external_libs qt)

  message(STATUS "Will build Qt (${QT_VERSION})")
endfunction()
