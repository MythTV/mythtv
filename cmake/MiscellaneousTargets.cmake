#
# Copyright (C) 2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT CMAKE_CROSSCOMPILING)
  ExternalProject_Add_Step(
    MythTV tests
    COMMAND cmake --build . -t test
    WORKING_DIRECTORY <BINARY_DIR>
    COMMENT "Running MythTV tests"
    USES_TERMINAL TRUE
    ALWAYS TRUE
    EXCLUDE_FROM_MAIN TRUE)
  ExternalProject_Add_StepTargets(MythTV tests)
endif()

ExternalProject_Add_Step(
  MythTV docs
  COMMAND cmake --build . -t docs
  WORKING_DIRECTORY <BINARY_DIR>
  COMMENT "Building MythTV docs"
  USES_TERMINAL TRUE
  ALWAYS TRUE
  EXCLUDE_FROM_MAIN TRUE)
ExternalProject_Add_StepTargets(MythTV docs)

#
# Build a script in the project binary directory to handle making the various
# tags files.  This lets it find all source files (as of the moment it is run)
# and makes it possible to provide the arguments to "etags" on the command line.
# (That's not possible with add_custom_command.)
#
configure_file(cmake/files/tags_tools.cmake.in tags_tools.cmake @ONLY)

function(add_misc_runtime_target NAME)
  add_custom_target(${NAME})
  add_custom_command(
    TARGET ${NAME}
    COMMAND ${CMAKE_COMMAND} -P tags_tools.cmake ${NAME}
    VERBATIM USES_TERMINAL)
endfunction()

add_misc_runtime_target(ctags)
add_custom_target(tags DEPENDS ctags)
add_misc_runtime_target(etags)
add_misc_runtime_target(gtags)
