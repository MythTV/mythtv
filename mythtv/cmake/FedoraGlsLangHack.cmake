#
# Copyright (C) 2022 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT LSB_RELEASE_ID STREQUAL "fedora")
  return()
endif()
if(LSB_RELEASE_VERSION_ID LESS_EQUAL 38)
  # glslang.pc on fedora is unusable due to leading spaces in the file. Find and
  # fix (which is a no-op elsewhere), then use.  This error seems to be fixed in
  # a later version of pkgconf.
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
else(LSB_RELEASE_VERSION_ID LESS_EQUAL 41)
  # Fedora fixed the above error in version 39, but they introduced a new one.
  # The version of glslang shipped in this versions no longer contains the HLSL
  # or OGLCompiler libraries, but those libraries are still listed in the
  # pkg-config file as libraries that need to be linked.
  # https://bugzilla.redhat.com/show_bug.cgi?id=2308904
  find_file(
    _pc_glslang glslang.pc
    PATHS /usr/lib64/pkgconfig /usr/lib/pkgconfig /usr/share/pkgconfig
          /usr/lib/x86_64-linux-gnu/pkgconfig
          /usr/lib64/x86_64-linux-gnu/pkgconfig)
  if(EXISTS ${_pc_glslang})
    file(READ ${_pc_glslang} glslang_content)
    if("${glslang_content}" MATCHES ".*HLSL.*")
      if(LSB_RELEASE_VERSION_ID EQUAL 39)
        set(_fix "14.0.0-2")
      else()
        set(_fix "14.3.0-2")
      endif()
      message(FATAL_ERROR "Please update glslang-devel to ${_fix} or later.")
    endif()
  endif()
endif()
