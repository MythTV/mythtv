#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_datacontracts test_datacontracts.cpp test_datacontracts.h)

target_include_directories(
  test_datacontracts
  PRIVATE # This directory
          .
          # mythtv/libs/libmythservicecontracts
          ../..
          # mythtv/libs
          ../../..)

target_link_libraries(test_datacontracts PUBLIC mythservicecontracts
                                                Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME DataContracts COMMAND test_datacontracts)
