#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_musicmetadata test_musicmetadata.cpp test_musicmetadata.h)

target_compile_definitions(
  test_musicmetadata PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(test_musicmetadata PRIVATE . ../..)

target_link_libraries(test_musicmetadata PUBLIC mythmetadata
                                                Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME MusicMetadata COMMAND test_musicmetadata)
