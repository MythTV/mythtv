add_library(mythfreesurround el_processor.cpp el_processor.h freesurround.cpp
                             freesurround.h)

target_compile_options(mythfreesurround PRIVATE -Wno-shadow)

target_include_directories(
  mythfreesurround
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(mythfreesurround PUBLIC PkgConfig::LIBAVCODEC
                                              PkgConfig::LIBAVUTIL mythbase)

install(TARGETS mythfreesurround LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
