#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#
include(BuildConfigString)

set(QT_DEFAULT_MAJOR_VERSION ${QT_VERSION_MAJOR})

#
# System must always provide Qt.
#
message(STATUS "Checking for ${QT_PKG_NAME} libraries")

if(NOT CMAKE_CROSSCOMPILING)
  set(_OTHER_REQUIRED DBus)
  if(${QT_VERSION_MAJOR} EQUAL 5)
    set(_OPTIONAL_COMPONENTS Script ScriptTools WebKit WebKitWidgets)
  endif()
elseif(ANDROID)
  if(${QT_VERSION_MAJOR} EQUAL 5)
    set(_OTHER_REQUIRED AndroidExtras)
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
  COMPONENTS Core
             Gui
             Network
             OpenGL
             Sql
             Test
             Widgets
             Xml
             ${_OTHER_REQUIRED}
  OPTIONAL_COMPONENTS ${_OPTIONAL_COMPONENTS})
message(STATUS "  Found ${QT_PKG_NAME}, version ${${QT_PKG_NAME}_VERSION}")

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
add_build_config(${QT_PKG_NAME}::Script "qtscript")
add_build_config(${QT_PKG_NAME}::Webkit "qtwebkit")

#
# Set properties
#
get_target_property(QMAKE_EXECUTABLE ${QT_PKG_NAME}::qmake IMPORTED_LOCATION)

if(TARGET ${QT_PKG_NAME}::DBus)
  target_compile_definitions(${QT_PKG_NAME}::DBus INTERFACE USING_DBUS)
  set(CONFIG_QTDBUS ON)
endif()

# Not all of the Qt6 include directories squirrel the private headers away into
# a sub-directory.  Look for paths like /usr/include/qt6/QtGui/6.4.3 and
# /usr/include/qt6/QtGui/6.4.3/QtGui and rewrite these up to point to the top
# include directory for that component.
function(clean_qt6_includes target)
  if(NOT TARGET ${target})
    return()
  endif()

  get_target_property(_DIRS ${target} INTERFACE_INCLUDE_DIRECTORIES)
  foreach(_DIR IN LISTS _DIRS)
    if(EXISTS ${_DIR})
      continue()
    endif()
    if(_DIR MATCHES "(.*)\/[0-9.]+(\/.*)?")
      set(_DIR ${CMAKE_MATCH_1})
    endif()
    list(APPEND _DIRS2 ${_DIR})
  endforeach()

  set_target_properties(${target} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                             "${_DIRS2}")
endfunction()

clean_qt6_includes(Qt6::CorePrivate)
clean_qt6_includes(Qt6::GuiPrivate)

if(TARGET ${QT_PKG_NAME}::GuiPrivate)
  target_compile_definitions(${QT_PKG_NAME}::GuiPrivate
                             INTERFACE USING_QTPRIVATEHEADERS)
endif()

if(TARGET ${QT_PKG_NAME}::Script)
  target_compile_definitions(${QT_PKG_NAME}::Script INTERFACE USING_QTSCRIPT)
  set(CONFIG_QTSCRIPT ON)
endif()

if(TARGET ${QT_PKG_NAME}::WebKit)
  set(CONFIG_QTWEBKIT ON)
  set(USING_QTWEBKIT ON)
endif()

#
# Figure out if Qt was build with GLES enabled.
#
message(STATUS "Checking for OpenGL ES")
find_file(
  _QTGUI_CONFIG
  NAMES QtGui/qtgui-config.h ${QT_PKG_NAME_LC}/QtGui/qtgui-config.h
  HINTS AAA ${CMAKE_INSTALL_PREFIX}/qt/include BBB)
if(_QTGUI_CONFIG)
  include(GetDefine)
  get_define(${_QTGUI_CONFIG} QT_OPENGL_ES_2
             "^#define +QT_OPENGL_ES_2 +([a-z]+)$")
  get_define(${_QTGUI_CONFIG} QT_FEATURE_opengles2
             "^#define +QT_FEATURE_opengles2 +([1-9]+)$")
else()
  message(STATUS "Can't find Qt GUI configuration file.  Assuming no OpenGLES.")
endif()
