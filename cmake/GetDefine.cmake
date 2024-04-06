include_guard()

#
# Given a regex, find a define in a header file and extract its value.
#
function(get_define _header _resultvar _symbolRegex)
  # Abolute or relative file name? If relative, find it.
  cmake_path(IS_ABSOLUTE _header _absolute)
  if(_absolute)
    set(_HEADER ${_header})
  else()
    find_file(_HEADER ${_header} PATHS ${CMAKE_PREFIX_PATH} REQUIRED)
  endif()

  # Extract lines containing the string
  file(STRINGS ${_HEADER} _line REGEX ${_symbolRegex})
  if(NOT _line STREQUAL "")
    # Now extract the value from the line, and set the parent variable.
    string(REGEX MATCH ${_symbolRegex} _matchedvalue ${_line})
    set(${_resultvar}
        ${CMAKE_MATCH_1}
        PARENT_SCOPE)
  else()
    # Set the parent variable to the special NOTFOUND result.
    set(${_resultvar}
        NOTFOUND
        PARENT_SCOPE)
    message(VERBOSE "No match of '${_symbolRegex}' in ${_HEADER}")
  endif()
  unset(_HEADER CACHE)
endfunction()
