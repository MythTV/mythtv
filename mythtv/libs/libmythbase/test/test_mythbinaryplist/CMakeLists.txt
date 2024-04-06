#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythbinaryplist test_mythbinaryplist.cpp
                                    test_mythbinaryplist.h)

target_include_directories(test_mythbinaryplist PRIVATE . ../..)

target_compile_definitions(
  test_mythbinaryplist PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(test_mythbinaryplist PUBLIC mythbase
                                                  Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME BinaryPLists COMMAND test_mythbinaryplist)
