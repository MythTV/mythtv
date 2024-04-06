#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

include(GetDefine)

#
# This module needs git functionality
#
find_package(Git)

#
# Parse a file in the style of a git EXPORTED_VERSION file.  This file contains
# one line describing the source version, and an optional second line with the
# branch name.
#
# ~~~
# This file has SOURCE_VERSION and BRANCH
# example SOURCE_VERSION="30d8a96"
# BRANCH examples from github
# BRANCH=" (HEAD -> master)"
# BRANCH=" (fixes/0.28)"
# BRANCH=" (tag: v0.28.1)"
# From a checkout they can be as follows:
# " (origin/fixes/0.28, fixes/0.28)"
# " (HEAD -> master, origin/master, origin/HEAD)"
# " (tag: v0.28.1)"
# ~~~
function(source_version_from_file filename version branch)
  # does the file exist?
  if(NOT EXISTS ${filename})
    message(FATAL_ERROR "File ${filename} doesn't exist.")
    return()
  endif()

  # Unset variables in case they aren't included in the file.
  unset(${version} PARENT_SCOPE)
  unset(${branch} PARENT_SCOPE)

  list(APPEND CMAKE_MESSAGE_INDENT "  ")

  # slurp the file
  message(DEBUG "Git version file ${filename}")
  file(STRINGS ${filename} lines REGEX "SOURCE_VERSION|BRANCH")
  foreach(line IN LISTS lines)
    message(DEBUG "  line: ${line}")
    if(line MATCHES "Format:")
      continue()
    elseif(line MATCHES "^ *SOURCE_VERSION *= *\" *([^ ]+) *\" *$")
      set(${version}
          "${CMAKE_MATCH_1}"
          PARENT_SCOPE)
    elseif(line MATCHES "^ *BRANCH *= *\" *\\( *(.*) *\\) *\" *$")
      set(${branch}
          "${CMAKE_MATCH_1}"
          PARENT_SCOPE)
    endif()
  endforeach()

  list(POP_BACK CMAKE_MESSAGE_INDENT)
endfunction()

#
# Clean up the returned branch string. This handles all the cases listed in the
# proir comment.
#
function(clean_branch_information branch)
  # Fix up branch
  if("${${branch}}" MATCHES "HEAD *-> *([A-Za-z0-9]*)")
    set(${branch}
        "${CMAKE_MATCH_1}"
        PARENT_SCOPE)
  elseif("${${branch}}" MATCHES "tag: *([A-Za-z0-9.]*)")
    set(${branch}
        "${CMAKE_MATCH_1}"
        PARENT_SCOPE)
  elseif("${${branch}}" MATCHES "(.*, )*(.+)")
    # Convert string to list, grab last item
    string(REGEX REPLACE " *, *" ";" branchList ${${branch}})
    list(POP_BACK branchList tmpBranch)
    if("${tmpBranch}" MATCHES "origin/(.*)")
      set(tmpBranch "${CMAKE_MATCH_1}")
    endif()
    set(${branch}
        "${tmpBranch}"
        PARENT_SCOPE)
  endif()
endfunction()

#
# Find version and branch information
#
# NOTE: This is not idempotent. It specifically depends on annotated tags, and
# adding a new tag will completely change the results of this function even
# though the code hasn't changed at all.
#
function(mythtv_find_source_version version version_major branch)

  # First check for a DESCRIBE file
  set(mythtv_source_dir ${PROJECT_SOURCE_DIR}/mythtv)
  set(filename "${mythtv_source_dir}/DESCRIBE")
  message(STATUS "Checking for version info in ${filename}")
  if(EXISTS ${filename})
    source_version_from_file(${filename} versionString branchName)
    message(STATUS "  version:${versionString} branch:${branchName}")
  endif()

  # Next if git is installed, ask it for the version and branch info. This will
  # succeed if the compile is running from a git tree and fail otherwise.
  if(NOT versionString AND GIT_EXECUTABLE)
    message(STATUS "Checking for version info from git")

    # Version
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --dirty
      OUTPUT_VARIABLE versionString
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

    if(NOT versionString)
      execute_process(
        COMMAND ${GIT_EXECUTABLE} describe
        OUTPUT_VARIABLE versionString
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()

    # Branch
    execute_process(
      COMMAND ${GIT_EXECUTABLE} branch --show-current
      OUTPUT_VARIABLE branchName
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

    # While this command appears to work for all cases, it does depend on the
    # specific git output format, so use it only if the simplier command does
    # not.
    if(NOT branchName)
      execute_process(
        COMMAND ${GIT_EXECUTABLE} branch --no-color --omit-empty --sort=refname
                --contains HEAD --merged HEAD
        OUTPUT_VARIABLE branchNames
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
      string(REGEX MATCHALL "[^\n\r]+" branchNamesList ${branchNames})
      list(GET branchNamesList 0 branchName)
      string(REGEX REPLACE "^[\\*\\+] " "" branchName ${branchName})
      string(STRIP ${branchName} branchName)
    endif()

    if(versionString)
      message(STATUS "  version:${versionString} branch:${branchName}")
    endif()
  endif()

  # Not in a git tree. Check for an EXPORTED_VERSION file.  This file is created
  # when the sources is exported from git using the "git archive" command.
  if(NOT versionString)
    set(filename "${mythtv_source_dir}/EXPORTED_VERSION")
    message(STATUS "Checking for version info in ${filename}")
    source_version_from_file(${filename} versionString branchName)

    # Fix up version
    if(versionString AND NOT versionString MATCHES "^v[0-9]")
      set(filename "${mythtv_source_dir}/SRC_VERSION")
      if(EXISTS ${filename})
        message(STATUS "Checking for version info in ${filename}")
        source_version_from_file(${filename} prefix dummy)
        set(versionString "${prefix}-${versionString}")
      endif()
    endif()

    clean_branch_information(branchName)
    if(versionString)
      message(STATUS "  version:${versionString} branch:${branchName}")
    endif()
  endif()

  # Sill haven't found a vesion string. Check for a SRC_VERSION file.
  if(NOT versionString)
    set(filename "${mythtv_source_dir}/SRC_VERSION")
    message(STATUS "Checking for version info in ${filename}")
    if(EXISTS ${filename})
      source_version_from_file(${filename} versionString branchName)
      message(STATUS "  version:${versionString}")
    endif()
  endif()

  # All the options for retrieving data have been tried. Validate the format of
  # the version string.
  if(versionString MATCHES "v([0-9]+).*")
    set(versionMajorString ${CMAKE_MATCH_1})
  else()
    message(WARNING "Invalid source version ${versionString}, must start "
                    "with v and a number. Setting it to unknown.")
    set(versionString "Unknown")
    set(versionMajorString "0")
    set(branchName "Unknown")
  endif()

  # Pass data back to caller
  set(${version}
      ${versionString}
      PARENT_SCOPE)
  set(${version_major}
      ${versionMajorString}
      PARENT_SCOPE)
  set(${branch}
      ${branchName}
      PARENT_SCOPE)
endfunction()

if(ENABLE_SELF_TEST)
  function(test_source_version_from_file filename versionE branchE)
    source_version_from_file(${CMAKE_CURRENT_LIST_DIR}/tests/${filename}
                             versionA branchA)
    if(NOT versionA STREQUAL versionE)
      message(
        FATAL_ERROR
          "source version test failed: file:${filename} expected:${versionE} actual: ${versionA}"
      )
    endif()
    clean_branch_information(branchA)
    if(DEFINED branchA AND NOT branchA STREQUAL branchE)
      message(
        FATAL_ERROR
          "source branch test failed: file:${filename} expected:'${branchE}' actual: '${branchA}'"
      )
    endif()
  endfunction()

  test_source_version_from_file("version_test_1" "20594a9a1e" "master")
  test_source_version_from_file("version_test_2" "a320543728" "cmake23")
  test_source_version_from_file("version_test_3" "aae56cb3c4" "v33.0")
  test_source_version_from_file("version_test_4" "v33.1" "")
  test_source_version_from_file("version_test_5" "v34-Pre" "")
endif()
