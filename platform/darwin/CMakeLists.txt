#
# Copyright (C) 2022-2024 David Hampton and John Hoyt
#
# See the file LICENSE_FSF for licensing information.
#

set(APP_NAME mythfrontend)
set(APP_EXE_NAME ${APP_NAME})
set(APP_EXES ${APP_NAME} mythpreviewgen mythutil mythmetadatalookup)

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
# From earlier builds in the superbuild:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/cmake")

#
# Read user options (part 1).  On macOS, several of these options affect the
# initialization performed by the project function.
#
include(MythOptions)
message(STATUS "Including user overrides ${MYTH_USER_OVERRIDES1}")
include(${MYTH_USER_OVERRIDES1} OPTIONAL)

#
# Describe this project
#
project(
  ${APP_NAME}
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
# Perform platform specific customization
#
include(platform/DarwinCustomization)

#
# The Qt macdeployqt program reads the headers of shared object files and
# automatically includes any of their required dependencies. Add the Qt install
# directory to the CMAKE_FIND_ROOT_PATH list so that macdeployqt can find
# the Qt libraries.
#
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_INSTALL_PREFIX}/qt)
list(APPEND CMAKE_FIND_ROOT_PATH ${LIBS_INSTALL_PREFIX}/qt)

#
# macdeployqt needs some help to find the installed mythtv libraries
#
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_INSTALL_PREFIX}/lib)

#
# Clean up the CMAKE_FIND_ROOT_PATH list
#
list(REMOVE_DUPLICATES CMAKE_FIND_ROOT_PATH)

#
# Set any package manger specific variables
#
if(MACPORTS)
  set(PKGMGR_PREFIX "${MACPORTS_PREFIX}")
  set(PKGMGR_LIB_DIR "${MACPORTS_PREFIX}/lib")
  set(FONT_PATH "${MACPORTS_PREFIX}/share/fonts")
elseif(HOMEBREW)
  set(PKGMGR_PREFIX "${HOMEBREW_PREFIX}")
  set(PKGMGR_LIB_DIR "${HOMEBREW_PREFIX}/lib")
  set(FONT_PATH "$ENV{HOME}/Library/fonts")
endif()

#
# Shorthand
#
set(MYTHTV_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(MYTHTV_FULL_DATADIR ${CMAKE_INSTALL_FULL_DATADIR}/mythtv)
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(BUILD_APP ${BUILD_DIR}/${APP_NAME}.app)
set(CONTENTS_DIR ${BUILD_DIR}/${APP_NAME}.app/Contents)
set(MACOS_DIR ${CONTENTS_DIR}/MacOS)
set(RSRC_DIR ${CONTENTS_DIR}/Resources)
set(RSRC_MYTHLIB_DIR ${CONTENTS_DIR}/Resources/lib/mythtv)
set(RSRC_MYTHBIN_DIR ${CONTENTS_DIR}/Resources/bin/)
set(FMWK_DIR ${CONTENTS_DIR}/Frameworks)
set(PLUGINS_DIR ${CONTENTS_DIR}/PlugIns)
set(PLIST_FILE ${CONTENTS_DIR}/Info.plist)

# qt 6.9.0 on homebrew moved the plugins and qml directories
# into share.  This might also occur in macports later so
# add logic to detect the move
set(QT_PLUGINS_DIR "${_QT_BASE}/plugins/")
if(NOT EXISTS "${QT_PLUGINS_DIR}")
  set(QT_PLUGINS_DIR "${_QT_BASE}/share/qt/plugins/")
endif()
set(QT_QML_DIR "${_QT_BASE}/qml/")
if(NOT EXISTS "${QT_QML_DIR}")
  set(QT_QML_DIR "${_QT_BASE}/share/qt/qml/")
endif()

# Locate Python for embedding the correct version from build time
#
message(STATUS "Setting up Python environment and variables")
find_package(Python3 COMPONENTS Interpreter Development)

# Find the actual python framework not the symlink or the location of any
# python virtual environments
if(DEFINED ENV{VIRTUAL_ENV} AND NOT $ENV{VIRTUAL_ENV} STREQUAL "")
  # If using a Virtual Environment, Python3_ROOT_DIR points to the virtual
  # environment so use _Python3_PATH2 instead
  execute_process(
    COMMAND zsh -c "greadlink -f ${_Python3_PATH2}"
      OUTPUT_VARIABLE PYTHON_ROOT_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
  # This uses Python3_ROOT_DIR as it points to the real python framework whereas
  execute_process(
    COMMAND zsh -c "greadlink -f ${Python3_ROOT_DIR}"
      OUTPUT_VARIABLE PYTHON_ROOT_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

set(PYTHON_DOT_VERSION ${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR})
set(PYTHON_BASENAME python${PYTHON_DOT_VERSION})
set(PYTHON_MACOS_EXE ${MACOS_DIR}/${PYTHON_BASENAME})
set(PYTHON_FMWK_DIR ${FMWK_DIR}/Python.framework)
set(PYTHON_FMWK_ROOT ${PYTHON_FMWK_DIR}/Versions/${PYTHON_DOT_VERSION})
set(PYTHON_FMWK_LIB_DIR ${PYTHON_FMWK_ROOT}/lib)
set(PYTHON_FMWK_FULL_DYLIB ${PYTHON_FMWK_ROOT}/Python)
set(PYTHON_RSRC_LIB_DIR ${RSRC_DIR}/lib/${PYTHON_BASENAME})
set(PYTHON_SOURCE_EXE ${PYTHON_ROOT_DIR}/Resources/Python.app/Contents/MacOS/Python)
set(PYTHON_SOURCE_DYLIB ${PYTHON_ROOT_DIR}/Python)

# Create a variable to correcting the path for the embedded python support libraries
if(DEFINED ENV{VIRTUAL_ENV} AND NOT $ENV{VIRTUAL_ENV} STREQUAL "")
  set(PYTHON_MYTHTV_SCRIPT_EXE $ENV{VIRTUAL_ENV}/bin/python3)
elseif(MACPORTS)
  set(PYTHON_MYTHTV_SCRIPT_EXE ${PKGMGR_PREFIX}/Library/Frameworks/Python.framework/Versions/${PYTHON_DOT_VERSION}/bin/python3)
elseif(HOMEBREW)
  set(PYTHON_MYTHTV_SCRIPT_EXE ${PKGMGR_PREFIX}/opt/python@${PYTHON_DOT_VERSION}/bin/python${PYTHON_DOT_VERSION})
endif()

#
# Activate the MacOS Bundling and set bundle version variables
#
message(STATUS "Setting up App Bundle structure and variables")
set(MACOSX_BUNDLE TRUE)
string(TIMESTAMP YEARSTAMP "%Y")
set(MACOSX_BUNDLE_BUNDLE_NAME ${APP_NAME})
set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2023-${YEARSTAMP} MythTV Dev Team")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.mythtv.${APP_NAME}")
set(MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "${MACOSX_BUNDLE_GUI_IDENTIFIER}")
set(MACOSX_BUNDLE_ICON_FILE "${APP_NAME}.icns")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR})

#
# Create the directory structure needed for packaging.
#
file(MAKE_DIRECTORY ${BUILD_DIR} ${MACOS_DIR} ${FMWK_DIR} ${RSRC_DIR} ${RSRC_MYTHLIB_DIR} ${RSRC_MYTHBIN_DIR})
if(MYTH_BUILD_PLUGINS)
  file(MAKE_DIRECTORY ${PLUGINS_DIR} )
endif()
#
# Copy in support files that do not need to be linked via macdeployqt
# Start with the mythtv/share directory into the Resource directory.
#
message(STATUS "Copying in MythTV support files")
message(VERBOSE "  Copying 'share' directory")
file(COPY ${MYTHTV_FULL_DATADIR} DESTINATION ${RSRC_DIR}/share)

# Copy the perl support files into Resources/lib.
message(VERBOSE "  Copying perl files")
file(COPY ${MYTHTV_INSTALL_PREFIX}/lib/perl5 DESTINATION ${RSRC_DIR}/lib)

## Copy the python support files into Resources/lib.
# If using a Virtual Environment, all of the necessary site-packages should
# already have been into the install prefix as well
if(EXISTS ${MYTHTV_INSTALL_PREFIX}/lib/${PYTHON_BASENAME})
  message(VERBOSE "  Copying python files")
  file(COPY ${MYTHTV_INSTALL_PREFIX}/lib/${PYTHON_BASENAME} DESTINATION ${RSRC_DIR}/lib)
endif()

# Copy the application icon into the Resources directory.
message(VERBOSE "  Copying icons")
file(COPY ${APP_NAME}.icns DESTINATION ${RSRC_DIR})

# Copy the application startup script and mythsources into the Resources
# directory. The mythsources files defines any necessary application
# environmental variables and aliases.  Both need cmake to configure internal
# variables.
message(VERBOSE "  Copying startup script and python alias")
configure_file("${CMAKE_SOURCE_DIR}/startup_script.zsh.in" "${MACOS_DIR}/${APP_NAME}.zsh" @ONLY)

# Update the files in share (e.g. python shebang's) to point to internally to the App
# Run theses as separate commands in case one fails
message(VERBOSE "  Fixing python shebangs")
# This first case should find the correct paths set by the myth build process
execute_process(
  COMMAND zsh -c "LC_ALL=C find ${RSRC_DIR} -type f -exec sed -i '' \"s^${PYTHON_MYTHTV_SCRIPT_EXE}.*^${PYTHON_BASENAME}^g\" {} + ")
# this second case is a bit more brute force
execute_process(
  COMMAND zsh -c "LC_ALL=C find ${RSRC_DIR} -type f -exec sed -i '' \"s^#!.*/python3.*^#!${PYTHON_BASENAME}^g\" {} + ")
execute_process(
  COMMAND zsh -c "LC_ALL=C find ${RSRC_DIR} -type f -exec sed -i '' \"s^${MYTHTV_INSTALL_PREFIX}^../Resources^g\" {} + ")

message(VERBOSE "  Copying fonts")
# Find and copy in the Dejavu and liberation fonts
execute_process(
  COMMAND zsh -c "find ${FONT_PATH} -name \"Deja*.ttf\" -o -name \"Liberation*.ttf\""
  OUTPUT_VARIABLE FONT_LIST
  OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE "\n" ";" FONT_LIST ${FONT_LIST})
foreach(FONT ${FONT_LIST})
    file(COPY ${FONT} DESTINATION ${RSRC_DIR}/share/mythtv/fonts)
endforeach()

# This function searches for the mythtv executable and library locations.
# The output is divided into rpath'd files and hard-linked files.
# These need different handling when conotpying into the app bundle
function(find_libraries_pathnames EXES_IN PATHS_OUT MISSING_OUT)
  # Possible locations for additional libraries.
  set(SRCHDIRS
      ${MYTHTV_INSTALL_PREFIX}/bin/
      ${MYTHTV_INSTALL_PREFIX}/lib/
      ${MYTHTV_INSTALL_PREFIX}/lib/mythtv/plugins/
      ${LIBS_INSTALL_PREFIX}/lib/
      ${PKGMGR_LIB_DIR})
  list(REMOVE_DUPLICATES SRCHDIRS)

  # Find the full path for each of the mythtv generated or rpath linked libraries
  foreach(EXENAME IN LISTS ${EXES_IN})
    # extract out all mythtv / rpath libraries from the NAME executable or dylib
    # use sed to cut out the text after the library name
    execute_process(
      COMMAND zsh -c "otool -L \"${MYTHTV_INSTALL_PREFIX}/bin/${EXENAME}\" |grep -e rpath -e \"${MYTHTV_INSTALL_PREFIX}\"| sed 's^\ (.*^^'"
      OUTPUT_VARIABLE OTOOL_OUT
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    # otool's outputs are separated with line breaks and a tab per library, so
    # we need to replace those with a delimiter to make it a list.
    string(REPLACE "@rpath/" "" OTOOL_OUT ${OTOOL_OUT})
    string(REPLACE "\n\t" ";" OTOOL_OUT ${OTOOL_OUT})
    # remove the executable from the list
    list(REMOVE_AT OTOOL_OUT 0)
    list(APPEND FILENAMES ${OTOOL_OUT})
  endforeach()
  list(REMOVE_DUPLICATES FILENAMES)
  foreach(NAME IN LISTS FILENAMES)
    # if the LIBNAME exists, this library is hard linked and does not need any
    # rpath search / handling
    if(EXISTS ${NAME})
      set(FOUND TRUE)
    # otherwise, this was an rpath library and needs to be located
    else()
      set(FOUND FALSE)
      foreach(SRCHDIR IN LISTS SRCHDIRS)
        if(EXISTS ${SRCHDIR}/${NAME})
          set(NAME ${SRCHDIR}/${NAME})
          set(FOUND TRUE)
          break()
        endif()
      endforeach()
    endif()
    if(NOT FOUND)
      list(APPEND MISSING ${NAME})
    endif()
    list(APPEND PATHS ${NAME})
  endforeach()
  set(${PATHS_OUT}
      ${PATHS}
      PARENT_SCOPE)
  set(${MISSING_OUT}
      ${MISSING}
      PARENT_SCOPE)
endfunction()

#
# create the dummy app bundle's target
#
if(QT_VERSION_MAJOR EQUAL 5)
  # IMPORTANT: All the variables necessary to describe the APP must be set
  # before this include.
  find_package(Qt5 COMPONENTS Core REQUIRED)
  get_target_property(qmake_executable Qt5::qmake IMPORTED_LOCATION)
  get_filename_component(_qt_bin_dir "${qmake_executable}" DIRECTORY)

  # Create the dummy target which will be overwritten later
  find_package( Qt5Widgets REQUIRED )
  add_executable(${APP_NAME} MACOSX_BUNDLE dummy.cpp)
  target_link_libraries(${APP_NAME} Qt5::Core Qt5::Widgets)

elseif(QT_VERSION_MAJOR EQUAL 6)
  find_package(Qt6 COMPONENTS Core REQUIRED)
  get_target_property(qmake_executable Qt6::qmake IMPORTED_LOCATION)
  get_filename_component(_qt_bin_dir "${qmake_executable}" DIRECTORY)

  # Create the dummy target which will be overwritten later
  find_package( Qt6Widgets REQUIRED )
  add_executable(${APP_NAME} MACOSX_BUNDLE dummy.cpp)
  target_link_libraries(${APP_NAME} Qt6::Core Qt6::Widgets)
else()
  message(FATAL_ERROR "Unsupported Qt version ${QT_VERSION_MAJOR EQUAL}")
endif()

# Set default macdeployqt flags
set(QT_DARWIN_EXTRAS -verbose=1
                     -qmlimport=${QT_QML_DIR}
                     -libpath=${MYTHTV_INSTALL_PREFIX}/lib
                     -libpath=${PKGMGR_LIB_DIR}
                     -libpath=${_QT_BASE}/lib/
                     -libpath=${QT_PLUGINS_DIR}
                     -libpath=${QT_PLUGINS_DIR}/sqldrivers/)

# activate generation of the app bundle and point the application's rpath to
# the frameworks where all dylibs must be stored
set_target_properties(${APP_NAME} PROPERTIES
  MACOSX_BUNDLE TRUE
  INSTALL_RPATH "@executable_path/../Frameworks")

#
# Copy in the required executables linking their rpaths internally. This will
# also overwrite the dummy executable generated previously
#
foreach(EXENAME IN LISTS APP_EXES)
  set(EXE_FULL_PATH ${MYTHTV_INSTALL_PREFIX}/bin/${EXENAME})
  add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    COMMENT "  Copying in ${EXENAME}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${EXE_FULL_PATH}" ${MACOS_DIR}
    COMMAND install_name_tool -rpath "@loader_path/../lib" "@loader_path/../Frameworks" ${MACOS_DIR}/${EXENAME}
    COMMAND install_name_tool -delete_rpath "@loader_path/../lib64" ${MACOS_DIR}/${EXENAME}
    COMMAND install_name_tool -add_rpath "${PKGMGR_LIB_DIR}" ${MACOS_DIR}/${EXENAME}
    COMMAND ${CMAKE_COMMAND} -E create_symlink "../../MacOS/${EXENAME}" ${RSRC_MYTHBIN_DIR}/${EXENAME})
  list(APPEND QT_DARWIN_EXTRAS "-executable=${MACOS_DIR}/${EXENAME}")
endforeach()

#
# Find the directly linked or rpath'd dylibs from the executables.
#
find_libraries_pathnames(APP_EXES DARWIN_EXTRA_LIBS MISSING)
if(MISSING)
  message(FATAL_ERROR "Can't find the following libraries: ${MISSING}")
endif()
list(REMOVE_DUPLICATES DARWIN_EXTRA_LIBS)
list(SORT DARWIN_EXTRA_LIBS)

#
# Copy each of the linked libraries into the Frameworks directory. Also add a
# -executable link for each to tell macdeployqt to update each library to use
# the app framework and recurse on the libraries other dependencies.
#
foreach(LIB_PATH_FULL IN LISTS DARWIN_EXTRA_LIBS)
  get_filename_component(LIB_NAME ${LIB_PATH_FULL} NAME)
  add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    COMMENT "  Copying in ${LIB_NAME}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIB_PATH_FULL}" ${FMWK_DIR})
  # Add flags so that macdeployqt allows the dylibs to use the Framework
  list(APPEND QT_DARWIN_EXTRAS "-executable=${FMWK_DIR}/${LIB_NAME}")
endforeach()

# Add any plugins
# Plugins need special handling from regular libraries as they need to be
# installed in the Frameworks/PlugIns directory and cannot live in the Resources
# directory to pass code signing.
if(MYTH_BUILD_PLUGINS)
  execute_process(
    COMMAND zsh -c "find ${MYTHTV_INSTALL_PREFIX}/lib/mythtv/plugins -name \"*.dylib\""
    OUTPUT_VARIABLE PLUGINS_FOUND
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REPLACE "\n" ";" PLUGINS_FOUND ${PLUGINS_FOUND})
  list(APPEND DARWIN_PLUGIN_LIBS ${PLUGINS_FOUND})
  # Copy the plugins into the Frameworks/PlugIns directory
  foreach(LIB_PATH_FULL IN LISTS DARWIN_PLUGIN_LIBS)
    get_filename_component(LIB_NAME ${LIB_PATH_FULL} NAME)
    add_custom_command(
      TARGET ${APP_NAME} POST_BUILD
      COMMENT "Copying in ${LIB_NAME}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIB_PATH_FULL}" ${PLUGINS_DIR})
    # Add flags so that macdeployqt allows the plugins to use the Framework
    list(APPEND QT_DARWIN_EXTRAS "-executable=${PLUGINS_DIR}/${LIB_NAME}")
  endforeach()
  add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    COMMENT "  Creating mythplugins symlinks"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "../../../PlugIns" ${RSRC_MYTHLIB_DIR}/plugins)
endif()

#
# create a portable python framework. This needs to be done after the App
# bundle variables and structure is established
#
include(DarwinEmbedPython.cmake)
# Add all python .so files to the QT_DARWIN_EXTRAS for macdeployqt cleanup
list(APPEND QT_DARWIN_EXTRAS "-executable=${PYTHON_FMWK_FULL_DYLIB}")
list(APPEND QT_DARWIN_EXTRAS "-executable=${PYTHON_MACOS_EXE}")
foreach(SO_NAME IN LISTS PYTHON_SOS_FOUND)
  # Add flags so that macdeployqt allows the .sos to use the Framework
  list(APPEND QT_DARWIN_EXTRAS "-executable=${SO_NAME}")
endforeach()

# Homebrew needs some extra help finding libraries located in the Cellar
if(HOMEBREW)
  execute_process(
    COMMAND zsh -c "find ${HOMEBREW_PREFIX}/Cellar -name lib"
    OUTPUT_VARIABLE CELLAR_LIST
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REPLACE "\n" ";" CELLAR_LIST ${CELLAR_LIST})
  foreach(CELLAR ${CELLAR_LIST})
      list(APPEND QT_DARWIN_EXTRAS "-libpath=${CELLAR}")
  endforeach()
endif()

#
# An issue re-inserted by qt 6.9.0 introduces a bug where macdeployqt
# cannot locate certain QT libraries referenced via rpath where those
# libraries do not have an id field with rpath in the id.  Creating
# a symlink to the package manger directory in the directory
# where macdeployqt will be called is a documented work around.
# https://github.com/orgs/Homebrew/discussions/2823#discussioncomment-8735051
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${PKGMGR_LIB_DIR}" "${BUILD_DIR}/lib")

#
# Run macdeployqt to add both the QT frameworks and any missing libraries into
# the Application Bundle.
#
# NOTE: All of these commands must be run together to prevent a race condition
# where some are run out of order when cmake parallelizes the process
#
# NOTE: the macdeployqt signing capabilities cannot be used as any adjustment
#       to the app bundle after the macdeployqt call violates the code signing
#
# See the following site for more details on Info.plist:
#   https://developer.apple.com/library/archive/documentation/General/Reference/InfoPlistKeyReference/Introduction/Introduction.html
# This site helps explain the use of plistbuddy
#   https://www.marcosantadev.com/manage-plist-files-plistbuddy/
#
# Use fixAppBundle to correct dylibs macdeployqt misses to search for referenced
# libraries in the app bundle.
#
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
add_custom_command(
  TARGET ${APP_NAME} POST_BUILD
  COMMENT "  Running macdeployqt, this will take a while..."
  COMMAND ${MACDEPLOYQT_EXECUTABLE} "${APP_NAME}.app" ${QT_DARWIN_EXTRAS}
  COMMAND /usr/libexec/PlistBuddy -c \"Add :ATSApplicationFontsPath string share/mythtv/fonts\" ${PLIST_FILE}
  COMMAND /usr/libexec/PlistBuddy -c \"Set :CFBundleExecutable ${APP_NAME}.zsh\" ${PLIST_FILE}
  COMMAND /usr/libexec/PlistBuddy -c \"Add :CFBundleAllowMixedLocalizations bool true\" ${PLIST_FILE}
  COMMAND /usr/libexec/PlistBuddy -c \"Set :CFBundleSignature string com.mythtv.${APP_NAME}\" ${PLIST_FILE}
  COMMAND /usr/libexec/PlistBuddy -c \"Set :CSResourcesFileMapped false\" ${PLIST_FILE}
  COMMAND /usr/libexec/PlistBuddy -c \"Add :NSAppleScriptEnabled bool false\" ${PLIST_FILE}
  COMMAND /usr/libexec/PlistBuddy -c \"Add :NSPrincipalClass string NSApplication\" ${PLIST_FILE}
  COMMAND /usr/libexec/PlistBuddy -c \"Add :NSSupportsAutomaticGraphicsSwitching bool true\" ${PLIST_FILE}
  COMMAND ${CMAKE_COMMAND} -E echo  "-- Correcting App Bundle post macdeployqt"
  COMMAND zsh -c "${CMAKE_CURRENT_LIST_DIR}/fixAppBundle.zsh \"${BUILD_DIR}/${APP_NAME}.app\" \"${PKGMGR_PREFIX}\" \"${MYTHTV_INSTALL_PREFIX}\""
  WORKING_DIRECTORY ${BUILD_DIR})

#
# Bundle with CPack
# NOTE: any variable starting with CPACK_ gets passed to CPACK. This is a useful
#       mechanism to pass required data to CPACK that might otherwise get lost.
# NOTE: CPACK code signing is not used as it requests and identity field be
#       passed in but provides no provisions to set the field as far as I could
#       find
#
if(DARWIN_GENERATE_DISTRIBUTION)
  # CPack variables
  set(CPACK_SET_DESTDIR 1)
  set(CPACK_VERBATIM_VARIABLES 1)
  set(CPACK_PACKAGE_NAME ${APP_NAME})
  set(CPACK_PACKAGE_VENDOR "MythTV")
  set(CPACK_PACKAGE_FILE_NAME "${APP_NAME}")
  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${MYTHTV_BINARY_CHANGED})
  set(CPACK_PACKAGE_DIRECTORY cpack_output)
  set(CPACK_PACKAGE_EXECUTABLES "${APP_NAME};${APP_NAME}.app")
  set(CPACK_INSTALL_PREFIX / )
  set(CPACK_PACKAGING_INSTALL_PREFIX "${APP_NAME}.app")
  set(CPACK_MONOLITHIC_INSTALL TRUE)
  set(CPACK_COMMAND_HDIUTIL "${CMAKE_CURRENT_LIST_DIR}/hdiutil_repeat.zsh")

  # Custom Inputs to Bundle
  set(CPACK_BUNDLE_NAME ${APP_NAME})
  set(CPACK_APPLE_BUNDLE_ID ${MACOSX_BUNDLE_GUI_IDENTIFIER})
  set(CPACK_BUNDLE_PLIST ${PLIST_FILE})
  set(CPACK_BUNDLE_ICON_FILE "${CMAKE_SOURCE_DIR}/${APP_NAME}.icns}")
  set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/${APP_NAME}.icns")
  set(CPACK_DMG_VOLUME_NAME "${APP_NAME}")

  # Code signing is in PRE as it must occur before the dmg is built
  if(NOT DARWIN_SIGNING_ID STREQUAL "")
    set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/DarwinSignAPP.cmake)
    set(CPACK_DARWIN_SIGNING_ID "${DARWIN_SIGNING_ID}")
    set(CPACK_ENTITLEMENTS_PLIST "${CMAKE_SOURCE_DIR}/entitlement.plist")
  endif()

  # Notarization is in POST for the dmg as needs the dmg to be created
  if(NOT DARWIN_NOTARIZATION_KEYCHAIN STREQUAL "")
    set(CPACK_POST_BUILD_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/DarwinSignDMG.cmake)
    set(CPACK_DARWIN_NOTARIZATION_KEYCHAIN "${DARWIN_NOTARIZATION_KEYCHAIN}")
  endif()

  set(CPACK_GENERATOR "Bundle")

  set_source_files_properties("${APP_NAME}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

  include(CPack)
endif()
if(HOMEBREW OR NOT DARWIN_GENERATE_DISTRIBUTION)
  # If this isn't a build for distribution, Ad Hoc Sign the APP
  add_custom_command(
  TARGET ${APP_NAME} POST_BUILD
  COMMENT "  Ad Hoc Codesigning App Bundle post macdeployqt"
  COMMAND zsh -c "${CMAKE_CURRENT_LIST_DIR}/codesignApp.zsh \"${BUILD_DIR}/${APP_NAME}.app\" \"${CMAKE_SOURCE_DIR}/entitlement.plist\""
  WORKING_DIRECTORY ${BUILD_DIR})
endif()
#
# Install App to the install prefix.
#
install(
  TARGETS ${APP_NAME}
  BUNDLE DESTINATION . COMPONENT Runtime)
