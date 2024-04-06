# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindPerlModules
--------

Find the listed Perl modules

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``PerlModules_FOUND``
  true if the PerlModules headers and libraries were found
``PerlModules_COMPONENTS_FOUND``
  A list of of all the modules that were found
``PerlModules_COMPONENTS_FOUND``
  A list of of all the modules that were found
``PerlModules_COMPONENTS_MISSING``
  A list of of all the modules that were not found (required or optional)
``PerlModules_COMPONENTS_REQUIRED_MISSING``
  A list of of all the required modules that were not found
``PerlModules_COMPONENTS_OPTIONAL_MISSING``
  A list of of all the optional modules that were not found
``PerlModules_yyy_VERSION``
  The version number of the yyy module

#]=======================================================================]

function(try_perl_module MODULE RESULT OUTPUT ERROR)
  execute_process(
    COMMAND ${PERL_EXECUTABLE} -M${MODULE} -e "print $${MODULE}::VERSION"
            RESULTS_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error
    ERROR_STRIP_TRAILING_WHITESPACE)
  message(DEBUG "_module is ${_module}")
  message(DEBUG "_result is ${_result}")
  message(DEBUG "_output is ${_output}")
  message(DEBUG "_error is ${_error}")
  set(${RESULT}
      ${_result}
      PARENT_SCOPE)
  set(${ERROR}
      ${_error}
      PARENT_SCOPE)
  set(${OUTPUT}
      ${_output}
      PARENT_SCOPE)
endfunction()

if(NOT PERL_EXECUTABLE)
  find_package(Perl REQUIRED)
endif()

foreach(_module IN LISTS PerlModules_FIND_COMPONENTS)
  message(DEBUG "Checking module ${_module}")
  try_perl_module(${_module} _result _output _error)
  if(_result EQUAL 0)
    set(PerlModules_${module}_FOUND TRUE)
    list(APPEND PerlModules_COMPONENTS_FOUND _{module})
    set(_version_str "version unknown")
    if(_output MATCHES "([0-9]+(.[0-9]+)+)")
      set(PerlModules_${module}_VERSION ${CMAKE_MATCH_1})
      set(_version_str "version ${CMAKE_MATCH_1}")
    elseif(_module MATCHES "([A-Za-z]+::[A-Za-z]+)::[A-Za-z]+")
      set(_module2 ${CMAKE_MATCH_1})
      try_perl_module(${_module2} _result _output _error)
      if(_output MATCHES "([0-9]+(.[0-9]+)+)")
        set(PerlModules_${module}_VERSION ${CMAKE_MATCH_1})
        set(_version_str "${_module2} version ${CMAKE_MATCH_1}")
      endif()
    endif()
    message(STATUS "Found perl module ${_module} (${_version_str})")
    set(PerlModules_${module}_VERSION ${CMAKE_MATCH_1})
  else()
    list(APPEND PerlModules_COMPONENTS_MISSING _{module})
    if(PerlModules_FIND_REQUIRED_${_module})
      list(APPEND PerlModules_REQUIRED_MISSING _{module})
      message(STATUS "Missing required perl module ${_module}")
    else()
      list(APPEND PerlModules_OPTIONAL_MISSING _{module})
      message(STATUS "Missing optional perl module ${_module}")
    endif()
  endif()
endforeach()

if(NOT PerlModules_COMPONENTS_MISSING)
  set(PerlModules_FOUND TRUE)
endif()

mark_as_advanced(
  PerlModules_FOUND PerlModules_COMPONENTS_FOUND PerlModules_COMPONENTS_MISSING
  PerlModules_REQUIRED_MISSING PerlModules_OPTIONAL_MISSING)
