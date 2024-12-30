#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT TARGET SQLite::SQLite3)
  message(
    STATUS "Can't build test for RecordingExtender feature. Missing SQLite3.")
  return()
endif()

add_executable(
  test_recordingextender
  ../../recordingextender.cpp dummyscheduler.cpp dummyscheduler.h
  test_recordingextender.cpp test_recordingextender.h)

target_include_directories(test_recordingextender PRIVATE . ../..)

target_compile_definitions(
  test_recordingextender PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(test_recordingextender
                      PUBLIC mythtv Qt${QT_VERSION_MAJOR}::Test SQLite::SQLite3)

add_test(NAME RecordingExtender COMMAND test_recordingextender)
