#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT CMAKE_ANDROID_ARCH_ABI
   AND NOT ANDROID
   AND NOT ARM64)
  return()
endif()
set(ANDROID ON)

if(WIN32)
  message(FATAL_ERROR "WIN32 set in Android Initialization file.")
endif()

# Pass the -DANDROID flag to all sub-projects so that MythOptions can properly
# set the NDK/SDK values.
list(APPEND PLATFORM_ARGS "-DANDROID:BOOL=ON")

#
# Additional early configuration for android.
#
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION ${ANDROID_TARGET_SDK_VERSION})

#
# Check CMAKE_ANDROID_ARCH_ABI to ensure its one that has been tested.
#
set(_KNOWN_ARCH "arm64-v8a" "armeabi-v7a" "x86" "x86_64")
if(NOT CMAKE_ANDROID_ARCH_ABI IN_LIST _KNOWN_ARCH)
  message(
    STATUS
      "Android: Unknown ABI CMAKE_ANDROID_ARCH_ABI='${CMAKE_ANDROID_ARCH_ABI}'."
  )
  message(STATUS "Android: Known architectures: ${_KNOWN_ARCH}.")
  message(FATAL_ERROR "Quitting.")
endif()

#
# Handle legacy ARM64 argument
#
if(ARM64)
  unset(ARM64)
  set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
  list(FILTER CMDLINE_ARGS EXCLUDE REGEX "-DARM64=")
  list(APPEND CMDLINE_ARGS "-DCMAKE_ANDROID_ARCH_ABI:STRING=arm64-v8a")
endif()

#
# Handle legacy SDK argument
#
if(SDK)
  unset(SDK)
  set(CMAKE_SYSTEM_VERSION ${SDK})
  set(ANDROID_TARGET_SDK_VERSION ${SDK})
  list(FILTER CMDLINE_ARGS EXCLUDE REGEX "-DSDK=")
  list(APPEND CMDLINE_ARGS "-DCMAKE_SYSTEM_VERSION:STRING=${SDK}")
  list(APPEND CMDLINE_ARGS "-DANDROID_TARGET_SDK_VERSION:STRING=${SDK}")
else()
  set(CMAKE_SYSTEM_VERSION ${ANDROID_TARGET_SDK_VERSION})
  message(
    STATUS "Setting CMAKE_SYSTEM_VERSION to ${ANDROID_TARGET_SDK_VERSION}")
endif()
include(platform/AndroidEnsureSdkValue)

#
# Check any user specified LIBS directory for a misleading name.
#
if(NOT LIBS_INSTALL_PREFIX STREQUAL "")
  set(BAD_ARCH ${_KNOWN_ARCH})
  list(REMOVE_ITEM BAD_ARCH ${CMAKE_ANDROID_ARCH_ABI})
  foreach(BAD IN LISTS BAD_ARCH)
    if(${LIBS_INSTALL_PREFIX} MATCHES ${BAD})
      message(WARNING "Compiling for ${CMAKE_ANDROID_ARCH_ABI} "
                      "but LIBS_INSTALL_PREFIX contains ${BAD}.")
    endif()
  endforeach(BAD)
  unset(BAD_ARCH)
endif()
