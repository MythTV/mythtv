#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Maybe build libudfread.
#
function(find_or_build_udfread)
  if(LIBS_USE_INSTALLED)
    pkg_check_modules(LIBUDFREAD "libudfread>=1.1.1" QUIET IMPORTED_TARGET)
    if(TARGET PkgConfig::LIBUDFREAD)
      message(
        STATUS
          "Found libudfread ${LIBUDFREAD_VERSION} ${LIBUDFREAD_LINK_LIBRARIES}")
      set(ProjectDepends
          ${ProjectDepends} PkgConfig::LIBUDFREAD
          PARENT_SCOPE)
      return()
    endif()
  endif()

  message(STATUS "Will build libudfread (embedded)")
  if(LIBS_INSTALL_UDFREAD)
    set(CMDLINE_ARGS_UDFREAD ${CMDLINE_ARGS_LIBS})
  else()
    set(CMDLINE_ARGS_UDFREAD ${CMDLINE_ARGS})
  endif()
  ExternalProject_Add(
    udfread
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/mythtv/external/libudfread
    CMAKE_ARGS --no-warn-unused-cli ${CMDLINE_ARGS_UDFREAD} ${PLATFORM_ARGS}
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:PATH=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:PATH=${PKG_CONFIG_PATH}
    BUILD_ALWAYS ${LIBS_ALWAYS_REBUILD}
    USES_TERMINAL_BUILD TRUE
    DEPENDS ${ExternalDepends} ${ProjectDepends})
  set(ProjectDepends
      ${ProjectDepends} udfread
      PARENT_SCOPE)
endfunction()

find_or_build_udfread()
