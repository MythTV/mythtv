#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_videometadata test_videometadata.cpp test_videometadata.h)

target_include_directories(test_videometadata PRIVATE . ../..)

target_link_libraries(test_videometadata PUBLIC mythmetadata
                                                Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME VideoMetadata COMMAND test_videometadata)
