# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindPython3Modules
--------

Find the listed Python3 modules

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Python3Modules_FOUND``
  true if the Python3Modules headers and libraries were found
``Python3Modules_COMPONENTS_FOUND``
  A list of of all the modules that were found
``Python3Modules_COMPONENTS_MISSING``
  A list of of all the modules that were not found (required or optional)
``Python3Modules_COMPONENTS_REQUIRED_MISSING``
  A list of of all the required modules that were not found
``Python3Modules_COMPONENTS_OPTIONAL_MISSING``
  A list of of all the optional modules that were not found
``Python3Modules_yyy_VERSION``
  The version number of the yyy module

#]=======================================================================]
if(NOT Python3_EXECUTABLE)
  find_package(
    Python3
    COMPONENTS Interpreter
    REQUIRED)
endif()

foreach(_module IN LISTS Python3Modules_FIND_COMPONENTS)
  message(DEBUG "Checking module ${_module}")
  if(_module MATCHES "([A-Za-z]+)([=<>]*)(.*)")
    set(_mod_name "${CMAKE_MATCH_1}")
    set(_mod_op "${CMAKE_MATCH_2}")
    set(_mod_ver "${CMAKE_MATCH_3}")
  else()
    set(_mod_name "${_module}")
    set(_mod_op)
    set(_mod_ver)
  endif()
  execute_process(
    COMMAND echo "import ${_mod_name} as m; print(m.__version__)"
    COMMAND ${Python3_EXECUTABLE}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE _error
    ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT _result EQUAL 0)
    execute_process(
      COMMAND echo "import ${_mod_name} as m"
      COMMAND ${Python3_EXECUTABLE}
      RESULT_VARIABLE _result
      OUTPUT_VARIABLE _output
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_VARIABLE _error
      ERROR_STRIP_TRAILING_WHITESPACE)
  endif()
  message(DEBUG "_result is ${_result}")
  message(DEBUG "_output is ${_output}")
  message(DEBUG "_error is ${_error}")
  if(NOT _result EQUAL 0)
    list(APPEND Python3Modules_COMPONENTS_MISSING ${_mod_name})
    if(Python3Modules_FIND_REQUIRED_${_mod_name})
      list(APPEND Python3Modules_REQUIRED_MISSING ${_mod_name})
      message(STATUS "Missing required python module ${_mod_name}")
    else()
      list(APPEND Python3Modules_OPTIONAL_MISSING ${_mod_name})
      message(STATUS "Missing optional python module ${_mod_name}")
    endif()
    continue()
  endif()

  if(${_output} MATCHES "([0-9]+(\.[0-9]+(\.[0-9]+)?)?)")
    set(_version ${CMAKE_MATCH_1})
    if(_mod_ver)
      message(DEBUG "  Comparing requested ${_mod_ver} to actual ${_version})")
    endif()
    if(NOT _mod_op STREQUAL "")
      if((${_mod_op} STREQUAL "<" AND ${_version} VERSION_LESS ${_mod_ver})
         OR (${_mod_op} STREQUAL "<=" AND ${_version} VERSION_LESS_EQUAL
                                          ${_mod_ver})
         OR (${_mod_op} STREQUAL "=" AND ${_version} VERSION_EQUAL ${_mod_ver})
         OR (${_mod_op} STREQUAL ">" AND ${_version} VERSION_GREATER ${_mod_ver}
            )
         OR (${_mod_op} STREQUAL ">=" AND ${_version} VERSION_GREATER_EQUAL
                                          ${_mod_ver}))
        # Empty body. Easier to write the test this way.
      else()
        message(
          STATUS "Ignoring python module ${_module} (version: ${CMAKE_MATCH_1})"
        )
        continue()
      endif()
    endif()
    message(STATUS "Found python module ${_module} (version: ${CMAKE_MATCH_1})")
    set(Python3Modules_${_mod_name}_FOUND TRUE)
    list(APPEND Python3Modules_COMPONENTS_FOUND ${_mod_name})
    set(Python3Modules_${_mod_name}_VERSION ${CMAKE_MATCH_1})
    mark_as_advanced(Python3Modules_${_mod_name}_VERSION)
  elseif(NOT _mod_op STREQUAL "")
    message(
      STATUS "Ignoring python module ${_module} (version: required but missing)"
    )
    continue()
  else()
    message(STATUS "Found python module ${_module} (version: not parsed)")
    set(Python3Modules_${_mod_name}_FOUND TRUE)
    list(APPEND Python3Modules_COMPONENTS_FOUND ${_mod_name})
  endif()
endforeach()

if(NOT Python3Modules_COMPONENTS_MISSING)
  set(Python3Modules_FOUND TRUE)
endif()

mark_as_advanced(
  Python3Modules_FOUND Python3Modules_COMPONENTS_FOUND
  Python3Modules_COMPONENTS_MISSING Python3Modules_REQUIRED_MISSING
  Python3Modules_OPTIONAL_MISSING)
