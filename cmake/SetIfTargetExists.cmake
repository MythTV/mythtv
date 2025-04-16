#
# Copyright (C) 2025 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

include_guard(GLOBAL)

macro(set_if_target_exists var target)
  if(TARGET ${target})
    set(${var} ON)
  else()
    set(${var} OFF)
  endif()
endmacro()
