#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT TARGET SQLite::SQLite3)
  message(
    WARNING "Can't build test for RecordingExtender feature. Missing SQLite3.")
endif()

add_executable(
  test_videolist ../../upnpscanner.cpp ../../videofilter.cpp
                 ../../videolist.cpp test_videolist.cpp test_videolist.h)

target_include_directories(test_videolist PRIVATE . ../..)

target_link_libraries(test_videolist PUBLIC mythmetadata
                                            Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME VideoList COMMAND test_videolist)
