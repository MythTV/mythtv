#
# Copyright (C) 2022-2024 John Hoyt
#
# See the file LICENSE_FSF for licensing information.
#
if(NOT APPLE)
  return()
endif()

#
# Assumes these are set before calling
#   APP_NAME
#   MACOS_DIR
#   RSRC_DIR
#   PYTHON_ROOT_DIR
#   PYTHON_DOT_VERSION
#   PYTHON_BASENAME
#   PYTHON_RSRC_LIB_DIR
#   PYTHON_FMWK_DIR
#   PYTHON_FMWK_ROOT
#   PYTHON_FMWK_LIB_DIR
#   PYTHON_FMWK_FULL_DYLIB
#   PYTHON_MACOS_EXE
#   PYTHON_SOURCE_EXE
#   PYTHON_SOURCE_DYLIB

# create the necessary file structure
file(MAKE_DIRECTORY ${PYTHON_FMWK_DIR}
                    ${PYTHON_FMWK_ROOT}
                    ${PYTHON_FMWK_ROOT}/bin
                    ${PYTHON_FMWK_ROOT}/Resources
                    ${PYTHON_FMWK_LIB_DIR})

#
# Resources/lib/
#

message(VERBOSE "  Adding Embedded Python libraries into Resources")
# For the initial copy, do not include site-packages as this directory is
# is handled differently on macports and homebrew.
# Also avoid copying over any libpython*.a or libpython*.dylib files as
# they are symlinks which will need to be converted into symlinks to the
# Frameworks directory otherwise app notarization will fail.
file(COPY "${Python3_STDLIB}/" DESTINATION ${PYTHON_RSRC_LIB_DIR}
    PATTERN "*site-packages*" EXCLUDE
    PATTERN "*libpython*.a" EXCLUDE
    PATTERN "*libpython*.dylib" EXCLUDE)

# If using a Virtual Environment, all of the necessary site-packages should
# already have been copied into the install prefix when the MythTV bindings
# were installed.  Otherwise all site-packages from the Macports or Homebrew
# python frameworks must be copied into the app bundle
if( NOT DEFINED ENV{VIRTUAL_ENV} OR "$ENV{VIRTUAL_ENV}" STREQUAL "")
  message(VERBOSE "  Adding Embedded Python site-packages into Resources")
  # not all python files are required in the app bundle.  This list is meant to
  # prune known unnecessary files to keep the app bundle size down.
  list(APPEND PYTHON_EXCLUDES
    PATTERN "*ansible*" EXCLUDE
    PATTERN "*lint*" EXCLUDE
    PATTERN "*pip*" EXCLUDE
    PATTERN "*py2app*" EXCLUDE
    PATTERN "*test*" EXCLUDE
    PATTERN "*virtualenv*" EXCLUDE
    PATTERN "*YAML*" EXCLUDE
    PATTERN "*yaml*" EXCLUDE)

  if(MACPORTS)
    file(COPY "${Python3_STDLIB}/site-packages" DESTINATION ${PYTHON_RSRC_LIB_DIR}
      ${PYTHON_EXCLUDES})
  elseif(HOMEBREW)
    file(COPY "${HOMEBREW_PREFIX}/lib/${PYTHON_BASENAME}/site-packages" DESTINATION ${PYTHON_RSRC_LIB_DIR}
      ${PYTHON_EXCLUDES})
    execute_process(
      COMMAND zsh -c "cp -LRn ${HOMEBREW_PREFIX}/lib/${PYTHON_BASENAME}/site-packages ${PYTHON_RSRC_LIB_DIR}/site-packages")
  endif()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${PYTHON_BASENAME}" "${RSRC_DIR}/lib/python"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${PYTHON_BASENAME}" "${RSRC_DIR}/lib/python3")

# find python .so files located in the Resources directory
execute_process(
    COMMAND zsh -c "find ${PYTHON_RSRC_LIB_DIR} -name \"*.so\""
    OUTPUT_VARIABLE PYTHON_SOS_FOUND
    OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE "\n" ";" PYTHON_SOS_FOUND ${PYTHON_SOS_FOUND})
list(TRANSFORM PYTHON_SOS_FOUND STRIP)
list(REMOVE_DUPLICATES PYTHON_SOS_FOUND)

message(VERBOSE "  Adding Embedded Python Framework")
#
# Python Binaries
# Framework/Python.framework
# MacOS/pythonX.XX
#
# Copy the Info.plist into Python Framework Resources dir
file(COPY "${PYTHON_ROOT_DIR}/Resources/Info.plist" DESTINATION ${PYTHON_FMWK_ROOT}/Resources)

# Create Framework symlinks
execute_process(
  COMMAND ${CMAKE_COMMAND} -E create_symlink "../../../../../Resources/lib/${PYTHON_BASENAME}" "${PYTHON_FMWK_LIB_DIR}/${PYTHON_BASENAME}"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "../../../../../Resources/lib/${PYTHON_BASENAME}" "${PYTHON_FMWK_LIB_DIR}/python"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "../../../../../Resources/lib/${PYTHON_BASENAME}" "${PYTHON_FMWK_LIB_DIR}/python3"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "Versions/Current/Python" "${PYTHON_FMWK_DIR}/Python"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "Versions/Current/Resources" "${PYTHON_FMWK_DIR}/Resources"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${PYTHON_DOT_VERSION}" "${PYTHON_FMWK_DIR}/Versions/Current"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "../Python" "${PYTHON_FMWK_LIB_DIR}/lib${PYTHON_BASENAME}.dylib"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "../Python" "${PYTHON_FMWK_LIB_DIR}/libpython.dylib"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "../Python" "${PYTHON_FMWK_LIB_DIR}/libpython3.dylib")

# The python executable needs its linked libraries updated to the embedded
# framework use otool to find the old links and install_name_tool to update them
execute_process(
  COMMAND zsh -c "otool -L \"${PYTHON_SOURCE_EXE}\" | grep -e 'Python (' | sed 's@\ (.*@@'"
    OUTPUT_VARIABLE OTOOL_OUT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE "\t" "" OTOOL_OUT ${OTOOL_OUT})

# Copy in the python library and executables
# NOTE: The actual Python library and executable need be copied post_build
add_custom_command(
  TARGET ${APP_NAME} POST_BUILD
  COMMENT "Copying in python library and executable"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PYTHON_SOURCE_DYLIB}" "${PYTHON_FMWK_FULL_DYLIB}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PYTHON_SOURCE_EXE}" "${PYTHON_MACOS_EXE}"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${PYTHON_BASENAME}" "${MACOS_DIR}/python"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${PYTHON_BASENAME}" "${MACOS_DIR}/python3"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "../../../../../MacOS/${PYTHON_BASENAME}" "${PYTHON_FMWK_ROOT}/bin/python3"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "../../../../../MacOS/${PYTHON_BASENAME}" "${PYTHON_FMWK_ROOT}/bin/${PYTHON_BASENAME}"
  COMMAND install_name_tool -change ${OTOOL_OUT} "@executable_path/../Frameworks/Python.framework/Versions/${PYTHON_DOT_VERSION}/Python" "${PYTHON_MACOS_EXE}")
