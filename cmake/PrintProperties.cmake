# Get all propreties that cmake supports
if(NOT CMAKE_PROPERTY_LIST)
  execute_process(
    COMMAND cmake --help-property-list
    OUTPUT_VARIABLE CMAKE_PROPERTY_LIST
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Convert command output into a CMake list
  string(REGEX REPLACE ";" "\\\\;" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
  string(REGEX REPLACE "\n" ";" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
  list(REMOVE_DUPLICATES CMAKE_PROPERTY_LIST)
endif()

function(print_properties)
  message("CMAKE_PROPERTY_LIST = ${CMAKE_PROPERTY_LIST}")
endfunction()

function(print_target_properties target)
  if(NOT TARGET ${target})
    message(STATUS "There is no target named '${target}'")
    return()
  endif()

  foreach(property IN LISTS CMAKE_PROPERTY_LIST)
    string(REPLACE "<CONFIG>" "${CMAKE_BUILD_TYPE}" property ${property})

    if(property STREQUAL "LOCATION"
       OR property MATCHES "^LOCATION_"
       OR property MATCHES "_LOCATION$")
      continue()
    endif()

    get_property(
      was_set
      TARGET ${target}
      PROPERTY ${property}
      SET)
    if(was_set)
      get_target_property(value ${target} ${property})
      message("${target} ${property} = ${value}")
    endif()
  endforeach()
endfunction()
