#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(
  test_metadatagrabber ../../lyricsdata.cpp test_metadatagrabber.cpp
                       test_metadatagrabber.h)

target_include_directories(test_metadatagrabber PRIVATE . ../..)

target_link_libraries(test_metadatagrabber PUBLIC mythmetadata
                                                  Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME MetaDataGrabber COMMAND test_metadatagrabber)
