#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_HOST_SYSTEM_NAME
                                              STREQUAL "Linux")
  return()
endif()

#
# Read the /etc/os-release file
#
function(get_lsb_info)
  file(STRINGS "/etc/os-release" LINES REGEX "ID")
  foreach(LINE IN LISTS LINES)
    if(LINE MATCHES "^ID=['\"]?([^'\"]*)['\"]?$")
      set(HOST_LSB_RELEASE_ID ${CMAKE_MATCH_1})
    elseif(LINE MATCHES "^VERSION_ID=['\"]?([^'\"]*)['\"]?$")
      set(HOST_LSB_RELEASE_VERSION_ID ${CMAKE_MATCH_1})
    endif()
  endforeach()

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LSB_RELEASE_ID
        ${HOST_LSB_RELEASE_ID}
        PARENT_SCOPE)
    set(LSB_RELEASE_VERSION_ID
        ${HOST_LSB_RELEASE_VERSION_ID}
        PARENT_SCOPE)
  endif()
  set(HOST_LSB_RELEASE_ID
      ${HOST_LSB_RELEASE_ID}
      PARENT_SCOPE)
  set(HOST_LSB_RELEASE_VERSION_ID
      ${HOST_LSB_RELEASE_VERSION_ID}
      PARENT_SCOPE)
endfunction()

get_lsb_info()
