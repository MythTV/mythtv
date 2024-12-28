#
# Copyright (C) 2022-2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(CMAKE_ANDROID_ARCH_ABI)
  set(ANDROID ON)
endif()

# Can use cmake features introduced in 3.21.
cmake_minimum_required(VERSION 3.21)

#
# Validate parameters
#
foreach(_param IN ITEMS SUPER_VERSION MYTHTV_SOURCE_VERSION)
  if(${_param} STREQUAL "")
    message(FATAL_ERROR "${_param} is a required parameter")
  endif()
endforeach()

if(NOT DEFINED LIBS_INSTALL_PREFIX OR LIBS_INSTALL_PREFIX STREQUAL "")
  set(LIBS_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()

#
# Where to find MythTV provided modules
#
# From the sources:
list(APPEND CMAKE_MODULE_PATH "${SUPER_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# From from earlier builds in the superbuild:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/cmake")

#
# Read user options (part 1).  On android, several of these options affect the
# initialization performed by the project function.
#
include(MythOptions)
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES1}")
include(${MYTH_USER_OVERRIDES1} OPTIONAL)

#
# Describe this project
#
project(
  mythfrontend
  VERSION ${SUPER_VERSION}
  LANGUAGES CXX)
include(VersionInformation)

#
# Read user options (part 2)
#
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES2}")
include(${MYTH_USER_OVERRIDES2} OPTIONAL)

#
# Inject code from cmake provided modules. GNUInstallDirs uses
# CMAKE_INSTALL_PREFIX so make sure that's set properly first.
#
include(GNUInstallDirs)
include(DumpAllVariables)

#
# Require the C++17 standard, and allow compiler extensions.
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

#
# Set search paths
#
include(SetSearchPaths)

#
# Perform platform specific customizations.
#
include(platform/AndroidCrossCustomization)

#
# The Qt androiddeployqt program reads the headers of shared object files and
# automatically includes any of their required dependencies. Add the Qt install
# directory to the CMAKE_FIND_ROOT_PATH list so that androiddeployqt can find
# the Qt libraries.
#
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_INSTALL_PREFIX}/qt)
list(APPEND CMAKE_FIND_ROOT_PATH ${LIBS_INSTALL_PREFIX}/qt)

#
# androiddeployqt also searches for android-dependencies.xml files for each
# shared object file loaded.  These are normally installed in the lib directory,
# but androiddeployqt doesn't add that "/lib" when searching ROOT_PATHs.  Add
# "/lib" manually to CMAKE_FIND_ROOT_PATH so that androiddeployqt can find the
# mythtv dependency files.
#
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_INSTALL_PREFIX}/lib)

#
# Clean up the CMAKE_FIND_ROOT_PATH list
#
list(REMOVE_DUPLICATES CMAKE_FIND_ROOT_PATH)

# Get tags used in bundle names.
include(${CMAKE_ANDROID_NDK}/build/cmake/abis.cmake)

#
# In Qt5, these variables that will be directly referenced by the code to build
# an apk.  In Qt6, these variables are set as properties on the executable and
# are referenced that way.
#
execute_process(
  COMMAND "date" "+%Y%m%d"
  OUTPUT_VARIABLE _date
  OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set(_BUNDLE_NAME "${NDK_ABI_${CMAKE_ANDROID_ARCH_ABI}_ARCH}")

set(ANDROID_PACKAGE_SOURCE_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/android-package-source)
set(ANDROID_VERSION_CODE 1)
set(ANDROID_VERSION_NAME
    "${_date}-qt${QT_VERSION_MAJOR}-${_BUNDLE_NAME}-${MYTHTV_SOURCE_VERSION}")
set(QT_ANDROID_APPLICATION_ARGUMENTS "-v general")

#
# Copy android-package-source into the build directory.  This allows Qt version
# specific customization.
#
file(COPY ${SUPER_SOURCE_DIR}/mythtv/android-package-source DESTINATION .)
if(ANDROID_TARGET_SDK_VERSION GREATER_EQUAL 31)
  list(APPEND MANIFEST_APPLICATION_ACTIVITY_EXTRAS android:exported="false")
endif()
if(ANDROID_TARGET_SDK_VERSION GREATER 99)
  list(APPEND MANIFEST_APPLICATION_EXTRAS android:examplsTagHere="false")
endif()
list(JOIN MANIFEST_APPLICATION_EXTRAS " " MANIFEST_APPLICATION_EXTRAS_STR)
list(JOIN MANIFEST_APPLICATION_ACTIVITY_EXTRAS " "
     MANIFEST_APPLICATION_ACTIVITY_EXTRAS_STR)
configure_file(AndroidManifest.xml.${QT_VERSION_MAJOR}.in
               android-package-source/AndroidManifest.xml @ONLY)

#
# Shorthand
#
set(MYTHTV_FULL_DATADIR ${CMAKE_INSTALL_FULL_DATADIR}/mythtv)
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/android-build)
set(ASSETS_DIR ${BUILD_DIR}/assets/mythtv)
set(JNI_DIR ${BUILD_DIR}/jni)
set(LIBS_ARCH ${BUILD_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI})

#
# Create the directory structure needed for packaging.
#
file(MAKE_DIRECTORY ${ASSETS_DIR} ${JNI_DIR})

#
# Generate files
#
configure_file(jni/Android.mk.in ${JNI_DIR}/Android.mk COPYONLY)
configure_file(jni/Application.mk.in ${JNI_DIR}/Application.mk @ONLY)

#
# Copy necessary XML files into the android-build/assets directory.
#
set(_XML_FILES CDS_scpd.xml CMGR_scpd.xml MFEXML_scpd.xml MSRR_scpd.xml)
list(TRANSFORM _XML_FILES PREPEND ${MYTHTV_FULL_DATADIR}/)
file(COPY ${_XML_FILES} DESTINATION ${ASSETS_DIR})

#
# Copy the translations into the android-build/assets directory.
#
file(
  GLOB _translations
  LIST_DIRECTORIES FALSE
  ${MYTHTV_FULL_DATADIR}/i18n/mythfrontend*)
file(COPY ${_translations} DESTINATION ${ASSETS_DIR}/i18n)

#
# Copy the fonts, etc into the android-build/assets directory.
#
foreach(DIRNAME ${MYTHTV_FULL_DATADIR}/fonts ${MYTHTV_FULL_DATADIR}/locales
                ${MYTHTV_FULL_DATADIR}/metadata ${MYTHTV_FULL_DATADIR}/themes)
  if(EXISTS ${DIRNAME})
    file(COPY ${DIRNAME} DESTINATION ${ASSETS_DIR})
  endif()
endforeach()

#
# Copy additional libraries into the android-build/assets directory.
#

# Library needed by the Qt MySql plugin.
list(APPEND EXTRA_LIB_NAMES mariadb/libmariadb.so)

# Libraries needed by the MythTV plugins.
if(MYTH_BUILD_PLUGINS AND MYTH_BUILD_ANDROID_PLUGINS)
  # MythMusic
  list(
    APPEND
    EXTRA_LIB_NAMES
    libFLAC.so
    libogg.so
    libvorbis.so
    libvorbisenc.so
    libvorbisfile.so)
endif()

function(find_full_pathnames NAMES PATHS_OUT MISSING_OUT)
  # Possible locations for additional libraries.
  set(LIBDIRS
      ${CMAKE_INSTALL_PREFIX}/lib
      ${CMAKE_INSTALL_PREFIX}/lib/mythtv/plugins
      ${CMAKE_INSTALL_PREFIX}/lib64
      ${CMAKE_INSTALL_PREFIX}/lib64/mythtv/plugins
      ${LIBS_INSTALL_PREFIX}/lib
      ${LIBS_INSTALL_PREFIX}/lib64)
  list(REMOVE_DUPLICATES LIBDIRS)

  # Find the full pathname each of the libraries
  foreach(NAME IN LISTS ${NAMES})
    set(FOUND FALSE)
    foreach(LIBDIR IN LISTS LIBDIRS)
      if(EXISTS ${LIBDIR}/${NAME})
        list(APPEND PATHS ${LIBDIR}/${NAME})
        set(FOUND TRUE)
        break()
      endif()
    endforeach()
    if(NOT FOUND)
      list(APPEND MISSING ${NAME})
    endif()
  endforeach()

  set(${PATHS_OUT}
      ${PATHS}
      PARENT_SCOPE)
  set(${MISSING_OUT}
      ${MISSING}
      PARENT_SCOPE)
endfunction()

find_full_pathnames(EXTRA_LIB_NAMES ANDROID_EXTRA_LIBS MISSING)
if(MISSING)
  message(FATAL_ERROR "Can't find the following libraries: ${MISSING}")
endif()
list(SORT ANDROID_EXTRA_LIBS)

#
# Find and add the MythTV plugins to the list of shared objects.
#
if(MYTH_BUILD_PLUGINS AND MYTH_BUILD_ANDROID_PLUGINS)
  set(PLUGIN_NAMES
      # MythTV plugins
      libmythpluginmytharchive.so
      libmythpluginmythbrowser.so
      libmythpluginmythgame.so
      libmythpluginmythmusic.so
      libmythpluginmythnetvision.so
      libmythpluginmythnews.so
      libmythpluginmythzoneminder.so)
  find_full_pathnames(PLUGIN_NAMES PLUGINS_FOUND MISSING)
  list(APPEND ANDROID_EXTRA_LIBS ${PLUGINS_FOUND})
endif()

if(QT_VERSION_MAJOR EQUAL 5)
  # IMPORTANT: All the variables necessary to describe the APK must be set
  # before this include.  The android_deployment_settings.json file will be
  # generated during this include process.
  include(MythFindQt)

  #
  # Install the mythfrontend application
  #
  foreach(_app IN ITEMS mythfrontend)
    execute_process(
      COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_INSTALL_PREFIX}/bin/${_app}
        ${LIBS_ARCH}/lib${_app}_${CMAKE_ANDROID_ARCH_ABI}.so
        COMMAND_ERROR_IS_FATAL ANY)
  endforeach()

  #
  # Codesigning in Qt5 is done by adding arguments to the command line.
  #
  if(QT_ANDROID_SIGN_APK)
    set(BUNDLESIGN
        "--sign" ${QT_ANDROID_KEYSTORE_PATH} ${QT_ANDROID_KEYSTORE_ALIAS}
        "--storepass" ${QT_ANDROID_KEYSTORE_STORE_PASS})
  endif()

  #
  # This is a copy of the apk target from Qt5AndroidSupport.cmake.  It is here
  # to ensure that the JAVA_HOME environment variable is always set to the value
  # configured in these cmake file.  Otherwise the call to 'cmake --build
  # <builddir>' will attempt to use whatever is present (if anything) in the
  # invoker's environment.
  #
  add_custom_target(
    apk2
    COMMAND
      # cmake-format: off
      echo ${CMAKE_COMMAND} -E env JAVA_HOME=${JAVA_HOME}
        ${ANDROID_DEPLOY_QT}
        --input "${PROJECT_BINARY_DIR}/android_deployment_settings.json"
        --output "${PROJECT_BINARY_DIR}/android-build"
        --apk "${PROJECT_BINARY_DIR}/android-build/${PROJECT_NAME}.apk"
        --android-platform ${ANDROID_PLATFORM}
        ${BUNDLESIGN}
        --verbose
      # cmake-format: on
    COMMAND
      # cmake-format: off
      ${CMAKE_COMMAND} -E env JAVA_HOME=${JAVA_HOME}
        ${ANDROID_DEPLOY_QT}
        --input "${PROJECT_BINARY_DIR}/android_deployment_settings.json"
        --output "${PROJECT_BINARY_DIR}/android-build"
        --apk "${PROJECT_BINARY_DIR}/android-build/${PROJECT_NAME}.apk"
        --android-platform ${ANDROID_PLATFORM}
        ${BUNDLESIGN}
        --verbose
      # cmake-format: on
    DEPENDS ${ANDROID_EXTRA_LIBS}
    VERBATIM USES_TERMINAL)

  #
  # Force the apk to always be built
  #
  # By using these two targets, the apk isn't rebuilt at the install stage.
  add_custom_target(
    apktest1
    DEPENDS apk2
    COMMAND echo
    BYPRODUCTS "${PROJECT_BINARY_DIR}/android-build/${PROJECT_NAME}.apk")
  add_custom_target(
    apktest2 ALL
    DEPENDS "${PROJECT_BINARY_DIR}/android-build/${PROJECT_NAME}.apk")

  #
  # Set variables use by Qt5Core/Qt5AndroidSupport.cmake when creating the file
  # android_deployment_settings.json.
  #
  # These variables aren't currently set to anything:
  # ~~~
  #  ANDROID_DEPLOYMENT_DEPENDENCIES
  #  ANDROID_EXTRA_PLUGINS
  #  QML_IMPORT_PATH
  # ~~~
  #

elseif(QT_VERSION_MAJOR EQUAL 6)
  set(QT_ANDROID_PLATFORM ${ANDROID_PLATFORM})
  set(QT_ENABLE_VERBOSE_DEPLOYMENT ON)

  # Inject code from super-project provided modules (part 2). All the variables
  # necessary to describe the APK must be set before this include.
  #
  # IMPORTANT: The android_deployment_settings.json file is *NOT* generated with
  # this include.  It is generated by the call to qt_add_executable.
  include(MythFindQt)

  set(CMAKE_BUILD_TYPE Debug)
  if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
  endif()
  if(QT_KNOWN_POLICY_QTP0002)
    qt_policy(SET QTP0002 NEW)
  endif()
  if(QT_KNOWN_POLICY_QTP0003)
    qt_policy(SET QTP0003 NEW)
  endif()

  #
  # Create a dummy mythfrontend executable.  This is where all the magic happens
  # behind the scenes in Qt6 to build an APK.
  #
  qt_add_executable(mythfrontend dummy.cpp)
  set_source_files_properties(dummy.cpp PROPERTIES HEADER_FILE_ONLY ON)

  #
  # Overwrite the dummy mythfrontend application with the real application.
  #
  add_custom_command(
    TARGET mythfrontend
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_INSTALL_PREFIX}/bin/mythfrontend
      libmythfrontend_${CMAKE_ANDROID_ARCH_ABI}.so)

  # Qt6 creates the android-mythfrontend-deployment-settings.json file based
  # upon the target properties.
  set_target_properties(
    mythfrontend
    PROPERTIES QT_ANDROID_VERSION_CODE ${ANDROID_VERSION_CODE}
               QT_ANDROID_VERSION_NAME ${ANDROID_VERSION_NAME}
               QT_ANDROID_MIN_SDK_VERSION ${ANDROID_MIN_SDK_VERSION}
               QT_ANDROID_TARGET_SDK_VERSION ${ANDROID_TARGET_SDK_VERSION}
               QT_ANDROID_SDK_BUILD_TOOLS_REVISION
               ${ANDROID_SDK_BUILD_TOOLS_REVISION}
               QT_ANDROID_PACKAGE_SOURCE_DIR ${ANDROID_PACKAGE_SOURCE_DIR}
               QT_ANDROID_EXTRA_LIBS "${ANDROID_EXTRA_LIBS}")

  #
  # Codesigning in Qt6 is done by setting environment variables.
  #
  if(QT_ANDROID_SIGN_APK)
    set(ENV{QT_ANDROID_KEYSTORE_PATH} ${QT_ANDROID_KEYSTORE_PATH})
    set(ENV{QT_ANDROID_KEYSTORE_ALIAS} ${QT_ANDROID_KEYSTORE_ALIAS})
    set(ENV{QT_ANDROID_KEYSTORE_STORE_PASS} ${QT_ANDROID_KEYSTORE_STORE_PASS})
    set(ENV{QT_ANDROID_KEYSTORE_KEY_PASS} ${QT_ANDROID_KEYSTORE_KEY_PASS})
  endif()

  #
  # qmake/cmake variables:
  #
  # https://doc.qt.io/qt-6/qmake-variable-reference.html
  #
  # Where do the parts of the android-mythfrontend-deployment-settings.json file
  # fome come from?
  #
  # ~~~
  # sdk: ${ANDROID_SDK_ROOT}
  # sdkBuildToolsRevision <-- fn result or QT_ANDROID_SDK_BUILD_TOOLS_REVISION
  # ndk <-- ${CMAKE_ANDROID_NDK}
  # toolchain-prefix <-- "llvm"
  # tool-prefix <-- "llvm"
  # useLLVM <-- true
  # toolchain-version <-- ${CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION}
  # ndk-host: ${ANDROID_NDK_HOST_SYSTEM_NAME}
  # architectures <-- from target qt_android_abis property
  # deployment-dependencies <-- target QT_ANDROID_DEPLOYMENT_DEPENDENCIES property
  # android-extra-plugins <-- target _qt_android_native_extra_plugins property
  # android-extra-libs <-- target QT_ANDROID_EXTRA_LIBS property
  # android-system-libs-prefix <-- target QT_ANDROID_SYSTEM_LIBS_PREFIX property
  # android-package-source-directory <-- target QT_ANDROID_PACKAGE_SOURCE_DIR property
  # android-version-code <-- QT_ANDROID_VERSION_CODE property
  # android-version-name <-- QT_ANDROID_VERSION_NAME property
  # android-min-sdk-version <-- QT_ANDROID_MIN_SDK_VERSION property
  # android-target-sdk-version <-- QT_ANDROID_TARGET_SDK_VERSION property
  # android-no-deploy-qt-libs <-- QT_ANDROID_NO_DEPLOY_QT_LIBS property
  # application-binary <-- ${target_output_name}
  # android-application-arguments <-- ${QT_ANDROID_APPLICATION_ARGUMENTS}
  # qml-skip-import-scanning <-- true
  # rcc-binary
  # extraPrefixDirs <-- generated from CMAKE_FIND_ROOT_PATH
  #     - Will search here for XML files
  # extraLibraryDirs <--
  #     - Will search here for things starting with "lib/"
  # stdcpp-path <-- Fixed path substituting CMAKE_SYSROOT
  #     - Patched to use CMAKE_ANDROID_NDK_TOOLCHAIN_UNIFIED since this
  #       build doesn't use CMAKE_SYSROOT
  # ~~~
  #
  # androiddeployqt/main.cpp absolutePath(blah) looks here:
  # ~~~
  #   starts with "lib/" - extraLibraryDirs
  #   anything - extraPrefixDirs
  #   ends with "-android-dependencies.xml" : qtLibsDirectory
  #   starts with "jar/" - qtDataDirectory
  #   starts with "lib/" - qtLibsDirectory without "lib/"
  #   anthing - base qtInstallDirectory
  # ~~~

else()
  message(FATAL_ERROR "Unsupported Qt version ${QT_VERSION_MAJOR EQUAL}")
endif()

#
# Install at the top level.
#
install(
  FILES ${PROJECT_BINARY_DIR}/android-build/${PROJECT_NAME}.apk
  DESTINATION ${SUPER_BINARY_DIR}
  RENAME mythfrontend-${ANDROID_VERSION_NAME}.apk)
