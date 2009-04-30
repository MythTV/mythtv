
include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
set(mythTest_SRCS mythTest.cpp)
qt4_automoc(${mythTest_SRCS})
add_executable(mythTest ${mythTest_SRCS})
add_test(mythTest mythTest)
target_link_libraries(mythTest ${QT_QTTEST_LIBRARY})
