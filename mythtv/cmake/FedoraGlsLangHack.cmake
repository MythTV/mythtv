#
# Copyright (C) 2022 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT LSB_RELEASE_ID STREQUAL "fedora")
  return()
endif()

# glslang.pc on fedora is unusable due to leading spaces in the file. Find and
# fix (which is a no-op elsewhere), then use.
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/share/pkgconfig)
foreach(_name IN ITEMS glslang spirv)
  find_file(
    _pc_${_name} ${_name}.pc
    PATHS /usr/lib64/pkgconfig /usr/lib/pkgconfig /usr/share/pkgconfig
          /usr/lib/x86_64-linux-gnu/pkgconfig
          /usr/lib64/x86_64-linux-gnu/pkgconfig)
  if(EXISTS ${_pc_${_name}})
    execute_process(
      COMMAND sed "-e" "s/^ *//g" "${_pc_${_name}}"
      OUTPUT_FILE ${PROJECT_BINARY_DIR}/share/pkgconfig/${_name}.pc
                  COMMAND_ERROR_IS_FATAL ANY)
  endif()
endforeach()
