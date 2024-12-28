#
# Copyright (C) 2022-2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# Figure out which java version to use.  The priority order is:
#
# ~~~
# 1) The environment variable JAVA_HOME
# 2) The option variable MYTH_JAVA_HOME
# 3) The android studio installation
# 4) The system version
# ~~~
#
# (Have to put env variable in real variable to test it.)
set(_TMP_ENV $ENV{JAVA_HOME})
if(_TMP_ENV)
  message(STATUS "Using java from environment: ${_TMP_ENV}")
  set(JAVA_HOME ${_TMP_ENV})
elseif(MYTH_JAVA_HOME)
  message(STATUS "Using java from options: ${MYTH_JAVA_HOME}")
  set(JAVA_HOME ${MYTH_JAVA_HOME})
else()
  message(STATUS "Searching for java from android studio")
  foreach(DIR "jbr" "jre")
    set(DIR2 "$ENV{HOME}/Android/android-studio/${DIR}")
    if(EXISTS ${DIR2})
      message(STATUS "  Found java: ${DIR2}")
      set(JAVA_HOME ${DIR2})
      break()
    endif()
  endforeach()
  if(NOT JAVA_HOME)
    message(STATUS "  Not found")
  endif()
endif()
if(NOT JAVA_HOME)
  message(STATUS "Using java from system path")
endif()

# Now validate the java version.
find_package(Java 11 REQUIRED COMPONENTS Development)
if(NOT Java_FOUND)
  message(FATAL_ERROR "Can't find installed java")
elseif(QT_VERSION_MAJOR EQUAL 5)
  if(Java_VERSION VERSION_GREATER_EQUAL 18)
    message(
      FATAL_ERROR
        "Java version ${Java_VERSION} is too new to compile Qt5. Qt5 requires gradle 7.2 which only officially supports Java 8 through 16, but Java 17 will work.  Please set the JAVA_HOME environment variable to a suitable java version."
    )
  endif()
elseif(QT_VERSION_MAJOR EQUAL 6)
  set(_MAX_JAVA_SUPPORTED 21)
  if(Java_VERSION VERSION_GREATER_EQUAL 22)
    message(
      FATAL_ERROR
        "Java version ${Java_VERSION} is too new to compile Qt6. Qt6 requires gradle 8.5 which supports Java 8 through 21.  Please set the JAVA_HOME environment variable to a suitable java version."
    )
  endif()
else()
  message(FATAL_ERROR "Unsupported version of Qt: Qt${QT_VERSION_MAJOR}")
endif()

# Validate the cmake and NDK versions for Android.  According to "Professional
# CMake: A Practical Guide", the minimum version used to compile android
# applications should be CMake 3.21 and Android NDK 23.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_ANDROID_NDK_VERSION VERSION_LESS 23.0)
  message(
    FATAL_ERROR
      "Android NDK version 23.0 or better required. Found version ${CMAKE_ANDROID_NDK_VERSION}."
  )
endif()
set(ANDROID_HIGHEST_NDK_TESTED 27.1)
if(NOT CMAKE_ANDROID_NDK_VERSION VERSION_LESS_EQUAL
   ${ANDROID_HIGHEST_NDK_TESTED})
  message(
    FATAL_ERROR
      "Android builds have not been tested with version "
      "${CMAKE_ANDROID_NDK_VERSION} of the NDK. Please use "
      "versions ${ANDROID_HIGHEST_NDK_TESTED} or older.")
endif()

if(ANDROID_SDK_BUILD_TOOLS_REVISION)
  cmake_path(SET build_tools_dir NORMALIZE
             ${CMAKE_ANDROID_NDK}/../../build-tools)
  file(
    GLOB revisions
    RELATIVE "${build_tools_dir}"
    "${build_tools_dir}/*")
  if(NOT ANDROID_SDK_BUILD_TOOLS_REVISION IN_LIST revisions)
    list(JOIN revisions " " revisions_str)
    message(
      FATAL_ERROR
        "Invalid ANDROID_SDK_BUILD_TOOLS_REVISION value ${ANDROID_SDK_BUILD_TOOLS_REVISION}.  Installed versions are: ${revisions_str}"
    )
  endif()
endif()
