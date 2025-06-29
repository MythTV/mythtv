#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

#
# Define the library
#

add_library(
  mythfreemheg
  Actions.cpp
  Actions.h
  ASN1Codes.h
  BaseActions.cpp
  BaseActions.h
  BaseClasses.cpp
  BaseClasses.h
  Bitmap.cpp
  Bitmap.h
  DynamicLineArt.cpp
  DynamicLineArt.h
  Engine.cpp
  Engine.h
  freemheg.h
  Groups.cpp
  Groups.h
  Ingredients.cpp
  Ingredients.h
  Link.cpp
  Link.h
  Logging.h
  ParseBinary.cpp
  ParseBinary.h
  ParseNode.cpp
  ParseNode.h
  ParseText.cpp
  ParseText.h
  Presentable.cpp
  Presentable.h
  Programs.cpp
  Programs.h
  Root.cpp
  Root.h
  Stream.cpp
  Stream.h
  Text.cpp
  Text.h
  TokenGroup.cpp
  TokenGroup.h
  Variables.cpp
  Variables.h
  Visible.cpp
  Visible.h
  )

target_include_directories(
  mythfreemheg
  PRIVATE $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}> # For config.h
  PUBLIC # For including without directory name
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         # For including with directory name
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(mythfreemheg PUBLIC Qt${QT_VERSION_MAJOR}::Core
                                          Qt${QT_VERSION_MAJOR}::Gui)

install(TARGETS mythfreemheg LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
