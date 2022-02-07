#
# Copyright (C) 2022 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

macro(add_build_config target string)
  if(TARGET ${target})
    list(APPEND MYTHTV_BUILD_CONFIG_LIST "using_${string}")
  elseif(${target})
    list(APPEND MYTHTV_BUILD_CONFIG_LIST "using_${string}")
  endif()
endmacro()
