#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_lyrics test_lyrics.cpp test_lyrics.h)

target_include_directories(test_lyrics PRIVATE . ../..)

target_compile_definitions(
  test_lyrics PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(test_lyrics PUBLIC mythmetadata
                                         Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME Lyrics COMMAND test_lyrics)
