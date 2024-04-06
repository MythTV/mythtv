#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythgenerictree test_mythgenerictree.cpp
                                    test_mythgenerictree.h)

target_include_directories(test_mythgenerictree PRIVATE . ../..)

target_link_libraries(test_mythgenerictree PUBLIC mythui
                                                  Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME UIGenericTree COMMAND test_mythgenerictree)
