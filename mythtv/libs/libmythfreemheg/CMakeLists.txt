add_library(
  mythfreemheg
  Actions.cpp
  BaseActions.cpp
  BaseClasses.cpp
  Bitmap.cpp
  DynamicLineArt.cpp
  Engine.cpp
  Groups.cpp
  Ingredients.cpp
  Link.cpp
  ParseBinary.cpp
  ParseNode.cpp
  ParseText.cpp
  Presentable.cpp
  Programs.cpp
  Root.cpp
  Stream.cpp
  Text.cpp
  TokenGroup.cpp
  Variables.cpp
  Visible.cpp)

target_include_directories(
  mythfreemheg
  PRIVATE $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}> # For config.h
  PUBLIC # For including without directory name
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         # For including with directory name
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(mythfreemheg PUBLIC Qt${QT_VERSION_MAJOR}::Core
                                          Qt${QT_VERSION_MAJOR}::Gui mythbase)

install(TARGETS mythfreemheg LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
