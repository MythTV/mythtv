#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_audioconvert test_audioconvert.cpp test_audioconvert.h)

target_include_directories(test_audioconvert PRIVATE . ../..)

target_link_libraries(test_audioconvert PUBLIC myth Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME AudioConvert COMMAND test_audioconvert)
