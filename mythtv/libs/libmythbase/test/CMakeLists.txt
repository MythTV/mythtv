#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(CMAKE_CROSSCOMPILING)
  return()
endif()

add_subdirectory(test_iso639)
add_subdirectory(test_lcddevice)
add_subdirectory(test_logging)
add_subdirectory(test_mythbinaryplist)
add_subdirectory(test_mythcommandlineparser)
add_subdirectory(test_mythdate)
add_subdirectory(test_mythdbcon)
add_subdirectory(test_mythsorthelper)
add_subdirectory(test_mythsystem)
add_subdirectory(test_mythsystemlegacy)
add_subdirectory(test_mythtimer)
add_subdirectory(test_programinfo)
add_subdirectory(test_rssparse)
add_subdirectory(test_template)
add_subdirectory(test_unzip)
