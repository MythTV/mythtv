#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Find ccache
#
find_program(CCACHE_FOUND ccache)
find_program(DISTCC_FOUND distcc)
if(CCACHE_FOUND)
  message(STATUS "Using ccache for compiles")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
elseif(DISTCC_FOUND)
  message(STATUS "Using distcc for compiles")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE distcc)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK distcc)
endif()
