include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

set(myth_SRCS myth.cpp main.cpp)
qt4_automoc(${myth_SRCS})
add_executable(myth ${myth_SRCS})
target_link_libraries(myth ${QT_QTCORE_LIBRARY})
