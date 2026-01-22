#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#
include(BuildConfigString)
include(SetIfTargetExists)

set(QT_DEFAULT_MAJOR_VERSION ${QT_VERSION_MAJOR})

#
# System must always provide Qt.
#
message(STATUS "Checking for ${QT_PKG_NAME} libraries")

set(_REQUIRED_COMPONENTS
    Core
    Gui
    Network
    OpenGL
    Sql
    Test
    Widgets
    Xml)
if(NOT CMAKE_CROSSCOMPILING)
  list(APPEND _REQUIRED_COMPONENTS DBus)
  if(${QT_VERSION_MAJOR} EQUAL 5)
    if(ENABLE_QTWEBENGINE)
      set(_OPTIONAL_COMPONENTS Quick WebEngine WebEngineWidgets)
    endif()
  else()
    if(ENABLE_QTWEBENGINE)
      set(_OPTIONAL_COMPONENTS Quick WebEngineQuick WebEngineWidgets)
    endif()
  endif()
elseif(ANDROID)
  if(${QT_VERSION_MAJOR} EQUAL 5)
    list(APPEND _REQUIRED_COMPONENTS AndroidExtras)
  endif()
endif()

include(platform/AndroidEnsureSdkValue)

#
# Start: prevent finding system androiddeployqt
#
if(CMAKE_CROSSCOMPILING)
  set(_OLD_MODE_PROGRAM ${CMAKE_FIND_ROOT_PATH_MODE_PROGRAM})
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
endif(CMAKE_CROSSCOMPILING)

#
# Find Qt and all necessary modules.
#
find_package(
  ${QT_PKG_NAME} ${QT_MIN_VERSION_STR} NO_MODULE
  COMPONENTS ${_REQUIRED_COMPONENTS}
  OPTIONAL_COMPONENTS ${_OPTIONAL_COMPONENTS})
foreach(component ${_REQUIRED_COMPONENTS} ${_OPTIONAL_COMPONENTS})
  if(${${QT_PKG_NAME}${component}_FOUND})
    list(APPEND _components_found ${component})
  else()
    list(APPEND _components_missing ${component})
  endif()
endforeach()
list(JOIN _components_found " " _components_found_str)
if(_components_missing)
  list(JOIN _components_missing " " _components_missing_str)
else()
  set(_components_missing_str "(none)")
endif()
message(STATUS "  Found ${QT_PKG_NAME}, version ${${QT_PKG_NAME}_VERSION}")
message(STATUS "  Found components: ${_components_found_str}")
message(STATUS "  Missing components: ${_components_missing_str}")

#
# End: prevent finding system androiddeployqt
#
if(CMAKE_CROSSCOMPILING)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ${_OLD_MODE_PROGRAM})
endif(CMAKE_CROSSCOMPILING)

#
# Validate that the right androiddeployqt was found
#
if(ANDROID)
  if(QT_DIR MATCHES "^/usr")
    message(FATAL_ERROR "Found system androiddeplayqt in ${QT_DIR}")
  endif()
endif(ANDROID)

#
# Update build config
#
add_build_config(${QT_PKG_NAME}::DBus "qtdbus")
add_build_config(${QT_PKG_NAME}::GuiPrivate "qtprivateheaders")
add_build_config(${QT_PKG_NAME}::Quick "qtquick")
add_build_config(${QT_PKG_NAME}::WebEngine "qtwebengine")
add_build_config(${QT_PKG_NAME}::WebEngineWidgets "qtwebenginewidgets")

#
# Set properties
#
get_target_property(QMAKE_EXECUTABLE ${QT_PKG_NAME}::qmake IMPORTED_LOCATION)

set_if_target_exists(CONFIG_QTDBUS ${QT_PKG_NAME}::DBus)
set_if_target_exists(CONFIG_QTPRIVATEHEADERS ${QT_PKG_NAME}::GuiPrivate)
set_if_target_exists(CONFIG_QTWEBENGINE ${QT_PKG_NAME}::WebEngineWidgets)

#
# Set compilation options
#
version_to_number(QT_FIXED_VERSION_HEX
                  ${QT${QT_VERSION_MAJOR}_DEPRECATION_FIXED} TRUE)
if(${${QT_PKG_NAME}_VERSION} VERSION_GREATER_EQUAL 6.8)
  add_compile_definitions(QT_ENABLE_STRICT_MODE_UP_TO=${QT_FIXED_VERSION_HEX})
  message(STATUS "  Setting: -DQT_ENABLE_STRICT_MODE_UP_TO=${QT_FIXED_VERSION_HEX}")
elseif(${${QT_PKG_NAME}_VERSION} VERSION_GREATER_EQUAL 6.5)
  add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=${QT_FIXED_VERSION_HEX})
  message(STATUS "  Setting: -DQT_DISABLE_DEPRECATED_UP_TO=${QT_FIXED_VERSION_HEX}")
else()
  add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=${QT_FIXED_VERSION_HEX})
  message(STATUS "  Setting: -DQT_DISABLE_DEPRECATED_BEFORE=${QT_FIXED_VERSION_HEX}")
endif()

#
# Figure out if Qt was build with GLES enabled.
#
message(STATUS "Checking for OpenGL ES")
find_file(
  _QTGUI_CONFIG
  NAMES QtGui/qtgui-config.h ${QT_PKG_NAME_LC}/QtGui/qtgui-config.h
  HINTS AAA ${CMAKE_INSTALL_PREFIX}/qt/include ${QTGUI_CONFIG_EXTRA} BBB)
if(_QTGUI_CONFIG)
  include(GetDefine)
  get_define(${_QTGUI_CONFIG} QT_OPENGL_ES_2
             "^#define +QT_OPENGL_ES_2 +([a-z]+)$")
  get_define(${_QTGUI_CONFIG} QT_FEATURE_opengles2
             "^#define +QT_FEATURE_opengles2 +([1-9]+)$")
else()
  message(STATUS "Can't find Qt GUI configuration file.  Assuming no OpenGLES.")
endif()
