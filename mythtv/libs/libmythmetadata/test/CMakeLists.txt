#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(CMAKE_CROSSCOMPILING)
  return()
endif()

add_subdirectory(test_lyrics)
add_subdirectory(test_metadatagrabber)
add_subdirectory(test_musicmetadata)
add_subdirectory(test_musicutils)
add_subdirectory(test_videometadata)
