#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(CMAKE_CROSSCOMPILING)
  return()
endif()

add_subdirectory(test_mythgenerictree)
add_subdirectory(test_settings)
