#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythiowrapper test_mythiowrapper.cpp test_mythiowrapper.h)

target_compile_definitions(
  test_mythiowrapper PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(test_mythiowrapper PRIVATE . ../..)

target_link_libraries(test_mythiowrapper PUBLIC mythtv
                                                Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME IOWrapper COMMAND test_mythiowrapper)
