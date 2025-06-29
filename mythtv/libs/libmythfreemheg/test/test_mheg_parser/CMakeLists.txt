#
# Copyright (C) 2025 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mheg_parser test_mheg_parser.cpp test_mheg_parser.h
  ../../Actions.cpp
  ../../BaseActions.cpp
  ../../BaseClasses.cpp
  ../../Bitmap.cpp
  ../../Engine.cpp
  ../../Groups.cpp
  ../../Ingredients.cpp
  ../../Link.cpp
  ../../ParseBinary.cpp
  ../../ParseNode.cpp
  ../../ParseText.cpp
  ../../Programs.cpp
  ../../Root.cpp
  ../../Text.cpp
  ../../Variables.cpp
  ../../Visible.cpp
  ../../DynamicLineArt.cpp
  ../../TokenGroup.cpp
  ../../Stream.cpp
  ../../Presentable.cpp
)

target_include_directories(test_mheg_parser PRIVATE . ../..)

target_link_libraries(test_mheg_parser PUBLIC mythbase Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME MhegParser COMMAND test_mheg_parser)
