#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(ANDROID)
  return()
endif()

#
# Provide a target for the sub-directories to hook into, and ensure that this
# target will be built by making it a dependency of libmythbase.
#
add_custom_target(bindings)
add_dependencies(mythbase bindings)

# Add all the sub-directories
add_subdirectory(perl)
add_subdirectory(php)
add_subdirectory(python)
