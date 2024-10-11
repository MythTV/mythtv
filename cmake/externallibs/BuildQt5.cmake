#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Qt
#
# configure, C and C++ source
#
function(find_or_build_qt)
  #
  # Limited check for Qt. Just the core module.
  #
  find_package(Qt5 ${QT5_MIN_VERSION_STR} QUIET NO_MODULE COMPONENTS Core)
  if(TARGET Qt5::Core)
    if("${Qt5_DIR}" MATCHES "(.*/qt)(/.*)")
      set(DIR ${CMAKE_MATCH_1})
    endif()
    message(STATUS "Found Qt5 ${Qt5Core_VERSION_STRING} in ${DIR}")
    add_library(Qt5 SHARED IMPORTED)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(QT_VERSION "5.15.15")
  string(REGEX MATCH "^([0-9]+\.[0-9]+)" QT_MAJMIN ${QT_VERSION})
  set(QT_PREFIX "qt-${QT_VERSION}")
  set(QT_URL
      "https://download.qt.io/archive/qt/${QT_MAJMIN}/${QT_VERSION}/single/qt-everywhere-opensource-src-${QT_VERSION}.tar.xz"
  )
  set(QT_5.15.9_SHA256
      "26d5f36134db03abe4a6db794c7570d729c92a3fc1b0bf9b1c8f86d0573cd02f")
  set(QT_5.15.11_SHA256
      "7426b1eaab52ed169ce53804bdd05dfe364f761468f888a0f15a308dc1dc2951")
  set(QT_5.15.15_SHA256
      "b423c30fe3ace7402e5301afbb464febfb3da33d6282a37a665be1e51502335e")
  ExternalProject_Add(
    qt
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL ${QT_URL}
    URL_HASH SHA256=${QT_${QT_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 < ${PROJECT_SOURCE_DIR}/patches/${QT_PREFIX}.patch
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${QT_PLATFORM_CONFIGURE_ENV}
      <SOURCE_DIR>/configure # cmake-format: off
          -opensource -confirm-license
          ${QT5_PLATFORM_ARGS}
          $<IF:$<BOOL:${VERBOSE}>,,-silent>
          $<IF:$<CONFIG:Debug>,-debug,-release>
          -prefix ${LIBS_INSTALL_PREFIX}/qt
          -extprefix ${LIBS_INSTALL_PREFIX}/qt
          -hostprefix ${LIBS_INSTALL_PREFIX}/qt
          -shared
          --disable-rpath
          -no-warnings-are-errors
          -nomake examples
          -nomake tools
          -openssl-linked
          -plugin-sql-mysql
          -qt-sqlite
          -skip qt3d
          -skip qtactiveqt
          -skip qtcharts
          -skip qtconnectivity
          -skip qtdatavis3d
#          Android needs this for the qmlimportscanner tool. Windows
#          builds will skip it.
#         -skip qtdeclarative
          -skip qtdoc
          -skip qtgamepad
          -skip qtgraphicaleffects
          -skip qtimageformats
          -skip qtlocation
          -skip qtlottie
          -skip qtmacextras
          -skip qtmultimedia
          -skip qtnetworkauth
          -skip qtpurchasing
          -skip qtquick3d
          -skip qtquickcontrols
          -skip qtquickcontrols2
          -skip qtquicktimeline
          -skip qtremoteobjects
          -skip qtscript
          -skip qtscxml
          -skip qtsensors
          -skip qtserialbus
          -skip qtserialport
          -skip qtspeech
          -skip qtsvg
          -skip qttools
          -skip qttranslations
          -skip qtvirtualkeyboard
          -skip qtwayland
          -skip qtwebchannel
          -skip qtwebengine
          -skip qtwebglplugin
          -skip qtwebsockets
          -skip qtwebview
          -skip qtwinextras
          -skip qtx11extras
          -skip qtxmlpatterns
	  -system-harfbuzz
          -I "${LIBS_INSTALL_PREFIX}/include"
          -I "${LIBS_INSTALL_PREFIX}/include/mariadb"
          -L "${LIBS_INSTALL_PREFIX}/lib"
          -L "${LIBS_INSTALL_PREFIX}/lib64"
          -L "${LIBS_INSTALL_PREFIX}/lib/mariadb"
          -L "${LIBS_INSTALL_PREFIX}/lib64/mariadb"
          ${QT5_PLATFORM_LIBS_ARGS}
          # cmake-format: on
    BUILD_COMMAND ${CMAKE_COMMAND} -E env ${QT_PLATFORM_BUILD_ENV}
                  ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    INSTALL_COMMAND ${CMAKE_COMMAND} -E env ${QT_PLATFORM_INSTALL_ENV}
                    ${MAKE_EXECUTABLE} install
    USES_TERMINAL_CONFIGURE ON
    USES_TERMINAL_BUILD ON
    USES_TERMINAL_INSTALL ON
    DEPENDS ${after_libs})

  ExternalProject_Add_Step(
    qt fix_ro_files
    DEPENDEES build
    COMMAND chmod -R +w <SOURCE_DIR>/qtwebengine/src/3rdparty)

  add_dependencies(external_libs qt)

  message(STATUS "Will build Qt (${QT_VERSION})")
endfunction()
