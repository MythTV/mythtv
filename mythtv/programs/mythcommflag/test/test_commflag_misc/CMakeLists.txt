#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(
  test_commflag_misc
  test_commflag_misc.cpp test_commflag_misc.h)

target_include_directories(test_commflag_misc PRIVATE . ../..)

target_compile_definitions(
  test_commflag_misc PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(test_commflag_misc PUBLIC mythtv Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME CommFlagMisc COMMAND test_commflag_misc)
