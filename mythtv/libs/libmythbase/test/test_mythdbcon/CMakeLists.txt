#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythdbcon test_mythdbcon.cpp test_mythdbcon.h)

target_include_directories(test_mythdbcon PRIVATE . ../.. ../../..)

target_link_libraries(test_mythdbcon PUBLIC mythbase
                                            Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME DatabaseEscaping COMMAND test_mythdbcon)
