#
# Copyright (C) 2025 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_theme_version test_theme_version.cpp test_theme_version.h)

target_include_directories(test_theme_version PRIVATE . ../..)

target_compile_definitions(
  test_theme_version PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(test_theme_version PUBLIC mythbase Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME TestThemeVersion COMMAND test_theme_version)
