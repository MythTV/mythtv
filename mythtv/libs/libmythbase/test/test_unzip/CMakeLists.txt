#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_unzip test_unzip.cpp test_unzip.h)

target_include_directories(test_unzip PRIVATE . ../..)

target_compile_definitions(
  test_unzip PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(test_unzip PUBLIC mythbase Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME UnZip COMMAND test_unzip)
