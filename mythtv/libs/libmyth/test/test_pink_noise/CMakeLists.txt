#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_pink_noise test_pink_noise.cpp test_pink_noise.h)

target_include_directories(test_pink_noise PRIVATE . ../..)

target_link_libraries(test_pink_noise PUBLIC myth Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME PinkNoise COMMAND test_pink_noise)
