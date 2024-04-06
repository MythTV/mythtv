#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

function(is_read_only DIRNAME VAR)
  if(NOT DEFINED DIRNAME OR NOT DEFINED VAR)
    message(FATAL_ERROR "Bad arguments to ${CMAKE_CURRENT_FUNCTION}")
  endif()

  if(NOT EXISTS ${DIRNAME})
    set(${VAR}
        ON
        PARENT_SCOPE)
    return()
  endif()
  # ~~~
  # if file(TOUCH blah) fails the function exits
  # file(TOUCH ${DIRNAME}/.ignore)
  # ~~~
  execute_process(COMMAND ${CMAKE_COMMAND} -E touch ${DIRNAME}/.ignore
                  OUTPUT_QUIET ERROR_QUIET)
  if(NOT EXISTS ${DIRNAME}/.ignore)
    set(${VAR}
        ON
        PARENT_SCOPE)
    return()
  endif()
  file(REMOVE ${DIRNAME}/.ignore)
  set(${VAR}
      OFF
      PARENT_SCOPE)
endfunction()

function(ensure_dir_writable TYPE DIRNAME)
  if(NOT DEFINED TYPE OR NOT DEFINED DIRNAME)
    message(FATAL_ERROR "Bad arguments to ${CMAKE_CURRENT_FUNCTION}")
  endif()

  if(NOT EXISTS ${DIRNAME})
    file(MAKE_DIRECTORY ${DIRNAME})
  endif()
  # ~~~
  # if file(TOUCH blah) fails the function exits
  # file(TOUCH ${DIRNAME}/.ignore)
  # ~~~
  execute_process(COMMAND ${CMAKE_COMMAND} -E touch ${DIRNAME}/.ignore
                  OUTPUT_QUIET ERROR_QUIET)
  if(NOT EXISTS ${DIRNAME}/.ignore)
    message(
      FATAL_ERROR "${TYPE} directory ${DIRNAME} (or parent) is read-only.")
  endif()
  file(REMOVE ${DIRNAME}/.ignore)
endfunction()
