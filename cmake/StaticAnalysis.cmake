#
# Copyright (C) 2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
message(VERBOSE "Found clang-tidy: ${CLANG_TIDY_EXECUTABLE}")
find_program(RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy)
message(VERBOSE "Found run-clang-tidy: ${RUN_CLANG_TIDY_EXECUTABLE}")
find_program(CPPCHECK_EXECUTABLE NAMES cppcheck)
message(VERBOSE "Found cppcheck: ${CPPCHECK_EXECUTABLE}")

function(sa_super)
  function(sa_add_step project name)
    ExternalProject_Add_Step(
      ${project} ${name}
      COMMAND cmake --build . -t run-${name}
      DEPENDEES build
      WORKING_DIRECTORY <BINARY_DIR>
      USES_TERMINAL TRUE
      ALWAYS TRUE
      EXCLUDE_FROM_MAIN TRUE)
    ExternalProject_Add_StepTargets(${project} ${name})
  endfunction()

  function(sa_add_cppcheck project)
    ExternalProject_Add_Step(
      ${project} cppcheck
      COMMAND cmake --build . -t run-cppcheck
      DEPENDEES build
      WORKING_DIRECTORY <BINARY_DIR>
      BYPRODUCTS <BINARY_DIR>/cppcheck.xml
      USES_TERMINAL TRUE
      ALWAYS TRUE
      EXCLUDE_FROM_MAIN TRUE)
    ExternalProject_Add_StepTargets(${project} cppcheck)
  endfunction()

  # Get the binary directory locations for the mythtv and mythplugins
  # sub-projects.
  ExternalProject_Get_Property(MythTV BINARY_DIR)
  set(MythTV_BINARY_DIR ${BINARY_DIR})
  ExternalProject_Get_Property(MythPlugins BINARY_DIR)
  set(MythPlugins_BINARY_DIR ${BINARY_DIR})

  # CMake always builds a compile_commands.json file in the binary
  # directory.  Add support for combining these into a single
  # compile_commands.json file.
  add_custom_target(
    compdb
    COMMENT "Installing compile_commands.json in ${PROJECT_SOURCE_DIR}"
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/scripts/concatenate-compdb.sh
            ${PROJECT_SOURCE_DIR}
            ${MythTV_BINARY_DIR}
            ${MythPlugins_BINARY_DIR}
    BYPRODUCTS ${PROJECT_SOURCE_DIR}/compile_commands.json
    USES_TERMINAL)

  # Run the clang tidy program.  This forces the tidy program to be
  # run in each of the sub-projects.
  if(CLANG_TIDY_EXECUTABLE AND RUN_CLANG_TIDY_EXECUTABLE)
    sa_add_step(MythTV clang-tidy)
    if(MYTH_BUILD_PLUGINS AND TARGET MythPlugins)
      sa_add_step(MythPlugins clang-tidy)
      add_custom_target(
        run-clang-tidy
        COMMAND ${CMAKE_COMMAND} --build . -t MythTV-clang-tidy
        COMMAND ${CMAKE_COMMAND} --build . -t MythPlugins-clang-tidy
        USES_TERMINAL)
    else()
      add_custom_target(
        run-clang-tidy
        COMMAND ${CMAKE_COMMAND} --build . -t MythTV-clang-tidy
        USES_TERMINAL)
    endif()
  endif()

  # Run the cppcheck program.  This forces the cppcheck program to be
  # run in each of the sub-projects and then the results are combined
  # together.
  if(CPPCHECK_EXECUTABLE)
    sa_add_cppcheck(MythTV)
    ExternalProject_Get_Property(MythTV BINARY_DIR)
    if(MYTH_BUILD_PLUGINS AND TARGET MythPlugins)
      sa_add_cppcheck(MythPlugins)
      add_custom_target(
        run-cppcheck
        DEPENDS ${MythTV_BINARY_DIR}/cppcheck.xml
                ${MythPlugins_BINARY_DIR}/cppcheck.xml
        BYPRODUCTS ${PROJECT_SOURCE_DIR}/cppcheck.xml
        COMMAND
          ${CMAKE_COMMAND} -DDIR1=${MythTV_BINARY_DIR}
          -DDIR2=${MythPlugins_BINARY_DIR} -DTOPDIR=${PROJECT_SOURCE_DIR} -P
          ${CMAKE_CURRENT_LIST_DIR}/scripts/cppcheck-join-files.cmake
        USES_TERMINAL)
    else()
      add_custom_target(
        run-cppcheck
        DEPENDS ${MythTV_BINARY_DIR}/cppcheck.xml
        BYPRODUCTS ${PROJECT_SOURCE_DIR}/cppcheck.xml
        COMMAND
          ${CMAKE_COMMAND} -DDIR1=${MythTV_BINARY_DIR}
          -DTOPDIR=${PROJECT_SOURCE_DIR} -P
          ${CMAKE_CURRENT_LIST_DIR}/scripts/cppcheck-join-files.cmake
        USES_TERMINAL)
    endif()
  endif()
endfunction()

function(sa_project)
  #
  # Setup clang tidy. Copy the .clang-tidy file in case the build directory
  # isn't under the source directory.
  #
  configure_file(${SUPER_SOURCE_DIR}/.clang-tidy .clang-tidy COPYONLY)
  if(CLANG_TIDY_EXECUTABLE)
    if(RUN_CLANG_TIDY_EXECUTABLE)
      # Target for running tidy on all files at once.
      add_custom_target(
        run-clang-tidy COMMAND ${RUN_CLANG_TIDY_EXECUTABLE} -clang-tidy-binary
                               ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR})
    endif()

    if(ENABLE_CLANG_TIDY)
      # Run tidy on each file when its compiled.
      set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR})
      set(CMAKE_CXX_CLANG_TIDY_EXPORT_FIXES_DIR clang-tidy-fixes)
    endif()
  endif()

  if(CPPCHECK_EXECUTABLE)
    # Target for running cppcheck on all files at once.
    add_custom_target(
      run-cppcheck
      COMMAND ${CMAKE_COMMAND} -E echo "Run cppcheck on ${PROJECT_NAME} sources"
      COMMAND
        ${CPPCHECK_EXECUTABLE} --xml-version=2
        --project=${CMAKE_BINARY_DIR}/compile_commands.json --enable=all
        --std=c++17 --platform=unix64 --library=posix.cfg --library=qt.cfg
        # includes
        --includes-file=${SUPER_SOURCE_DIR}/.cppcheck-includes
        # suppressions
        --inline-suppr
        --suppressions-list=${SUPER_SOURCE_DIR}/.cppcheck-suppress
        --suppressions-list=${PROJECT_SOURCE_DIR}/.cppcheck-suppress
        --output-file=${CMAKE_BINARY_DIR}/cppcheck.xml
      DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
              ${SUPER_SOURCE_DIR}/.cppcheck-suppress
              ${PROJECT_SOURCE_DIR}/.cppcheck-suppress
      USES_TERMINAL)

    if(ENABLE_CPPCHECK)
      # Run cppcheck on each file when its compiled.
      set(CMAKE_CXX_CPPCHECK ${CPPCHECK_EXECUTABLE})
    endif()
  endif()
endfunction()

if(CMAKE_PROJECT_NAME STREQUAL "MythTV-SuperBuild")
  sa_super()
else()
  sa_project()
endif()
