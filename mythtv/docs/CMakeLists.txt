#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

find_program(DOXYGEN_EXECUTABLE NAMES doxygen)
message(VERBOSE "Found doxygen: ${DOXYGEN_EXECUTABLE}")
if(NOT DOXYGEN_EXECUTABLE)
  return()
endif()

#
# Link to the doxygen-create-developer-docs.cfg file so it can be easily
# referenced in the destination directory.
#
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/doxygen-create-developer-docs.cfg
     ${CMAKE_CURRENT_BINARY_DIR}/doxygen-create-developer-docs.cfg
     COPY_ON_ERROR SYMBOLIC)

#
# Create a version of the doxygen-paths.cfg.inc file in the destination
# directory, modified to handle an out-of-tree build.
#
set(SOURCES_LIST
    "${CMAKE_CURRENT_SOURCE_DIR}/doxygen-architecture-document.cpp"
    "${PROJECT_SOURCE_DIR}/libs"
    "${PROJECT_SOURCE_DIR}/programs" "${PROJECT_SOURCE_DIR}/../mythplugins")
list(JOIN SOURCES_LIST " " DOXYGEN_SOURCES)

set(DOXYGEN_EXCLUDES "${PROJECT_SOURCE_DIR}/libs/libmythmpeg2")

set(DOXYGEN_STRIP_FROM_PATH "${PROJECT_SOURCE_DIR}/../")

set(STRIP_FROM_INC_PATH_LIST
    "${PROJECT_SOURCE_DIR}/libs"
    "${PROJECT_SOURCE_DIR}/programs" "${PROJECT_SOURCE_DIR}/../")
list(JOIN STRIP_FROM_INC_PATH_LIST " " DOXYGEN_STRIP_FROM_INC_PATH)

configure_file(doxygen-paths.cfg.inc.in doxygen-paths.cfg.inc @ONLY)

# Command to create documentation
add_custom_command(
  OUTPUT doxygen-dev-docs/doxygen.stamp
  DEPENDS doxygen-create-developer-docs.cfg
  BYPRODUCTS doxygen-warnings.txt
  COMMAND ${CMAKE_COMMAND} -E echo
          "Generating API documentation.  This takes a long time..."
  COMMAND ${DOXYGEN_EXECUTABLE} doxygen-create-developer-docs.cfg
  COMMAND cmake -E touch doxygen-dev-docs/doxygen.stamp
  VERBATIM USES_TERMINAL)

# Target used to invoke command.
add_custom_target(docs DEPENDS doxygen-dev-docs/doxygen.stamp)
