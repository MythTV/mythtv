#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Program and macro to handle cleaning up the shebang at the start of python
# files.
#
set(PATHFIX ${CMAKE_CURRENT_SOURCE_DIR}/python_pathfix.py)

macro(fix_shebang_install_python NAME PYFILES)
  list(LENGTH PYFILES _count)
  message(DEBUG
          "fix_shebang_install_python processing ${_count} files for ${NAME}")
  foreach(_FILE IN LISTS PYFILES)
    message(DEBUG "fix_shebang_install_python ${_FILE}")
    cmake_path(GET _FILE PARENT_PATH _DIR)
    add_custom_command(
      OUTPUT ${_FILE}
      MAIN_DEPENDENCY ${_FILE}
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${_FILE}
              ${_FILE}
      COMMAND ${Python3_EXECUTABLE} ${PATHFIX} ${_FILE})
    list(APPEND PROCESSED_FILES ${CMAKE_CURRENT_BINARY_DIR}/${_FILE})
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${_FILE}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/${NAME}/${_DIR})
  endforeach()
endmacro()

#
# Process the subdirectories
#
add_subdirectory(database)
add_subdirectory(hardwareprofile)
add_subdirectory(internetcontent)
add_subdirectory(metadata)
