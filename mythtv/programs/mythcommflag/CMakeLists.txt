if(BUILD_TESTING)
  add_subdirectory(test)
endif()

add_executable(
  mythcommflag
  BlankFrameDetector.cpp
  BlankFrameDetector.h
  BorderDetector.cpp
  BorderDetector.h
  CannyEdgeDetector.cpp
  CannyEdgeDetector.h
  ClassicCommDetector.cpp
  ClassicCommDetector.h
  ClassicLogoDetector.cpp
  ClassicLogoDetector.h
  ClassicSceneChangeDetector.cpp
  ClassicSceneChangeDetector.h
  CommDetector2.cpp
  CommDetector2.h
  CommDetectorBase.cpp
  CommDetectorBase.h
  CommDetectorFactory.cpp
  CommDetectorFactory.h
  CustomEventRelayer.h
  EdgeDetector.cpp
  EdgeDetector.h
  FrameAnalyzer.cpp
  FrameAnalyzer.h
  Histogram.cpp
  Histogram.h
  HistogramAnalyzer.cpp
  HistogramAnalyzer.h
  LogoDetectorBase.h
  mythcommflag.cpp
  mythcommflag_commandlineparser.cpp
  mythcommflag_commandlineparser.h
  pgm.cpp
  pgm.h
  PGMConverter.cpp
  PGMConverter.h
  PrePostRollFlagger.cpp
  PrePostRollFlagger.h
  quickselect.h
  SceneChangeDetector.cpp
  SceneChangeDetector.h
  SceneChangeDetectorBase.h
  SlotRelayer.h
  TemplateFinder.cpp
  TemplateFinder.h
  TemplateMatcher.cpp
  TemplateMatcher.h)

target_include_directories(mythcommflag PRIVATE .)

target_link_libraries(
  mythcommflag PUBLIC PkgConfig::LIBAVCODEC PkgConfig::LIBAVUTIL myth mythtv
                      mythbase)

install(TARGETS mythcommflag RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
