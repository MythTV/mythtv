#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT ANDROID)
  return()
endif()

#
# Ensure that ANDROID_SDK is set properly.  This code will attempt to find the
# SDK given the path of the NDK.
#
if(NOT ANDROID_SDK)
  if(NOT CMAKE_ANDROID_NDK)
    message(FATAL_ERROR "CMAKE_ANDROID_NDK isn't set.")
  endif()
  set(_SDK ${CMAKE_ANDROID_NDK})
  cmake_path(GET _SDK ROOT_PATH _ROOT)
  while(NOT ${_SDK} STREQUAL ${_ROOT})
    cmake_path(GET _SDK PARENT_PATH _SDK)
    if(EXISTS ${_SDK}/platforms)
      set(ANDROID_SDK ${_SDK})
      break()
    endif()
  endwhile()
  if(NOT ANDROID_SDK)
    message(
      FATAL_ERROR "Can't find SDK starting from from NDK ${CMAKE_ANDROID_NDK}")
  endif()
endif()

# For Qt6
set(ANDROID_SDK_ROOT ${ANDROID_SDK})
set(ANDROID_NDK_HOST_SYSTEM_NAME ${CMAKE_ANDROID_NDK_TOOLCHAIN_HOST_TAG})
