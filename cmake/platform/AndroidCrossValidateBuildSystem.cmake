#
# Copyright (C) 2022-2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

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
