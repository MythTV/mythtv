#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Encapsulate this code as a function so all the variables are local.
#
function(print_build_name)
  file(STRINGS "/etc/os-release" LINES REGEX "(NAME|VERSION)")
  foreach(LINE IN LISTS LINES)
    if(LINE MATCHES "^PRETTY_NAME=['\"]?([^'\"]*)['\"]?$")
      set(PRETTY_NAME ${CMAKE_MATCH_1})
      break()
    elseif(LINE MATCHES "^NAME=['\"]?([^'\"]*)['\"]?$")
      set(VENDOR ${CMAKE_MATCH_1})
    elseif(LINE MATCHES "^VERSION=['\"]?([^'\"]*)['\"]?$")
      set(VERSION ${CMAKE_MATCH_1})
    endif()
  endforeach()

  if(NOT PRETTY_NAME)
    set(PRETTY_NAME "${VENDOR} ${VERSION}")
  endif()

  message(STATUS "Build system is ${PRETTY_NAME}")
endfunction()

# Print the build systen name.
print_build_name()
