#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(CMAKE_CROSSCOMPILING)
  return()
endif()

add_subdirectory(test_avcinfo)
add_subdirectory(test_bitreader)
add_subdirectory(test_copyframes)
add_subdirectory(test_eitfixups)
add_subdirectory(test_frequencies)
add_subdirectory(test_iptvrecorder)
add_subdirectory(test_mheg_dsmcc)
add_subdirectory(test_mpegtables)
add_subdirectory(test_mythiowrapper)
add_subdirectory(test_subtitlescreen)
