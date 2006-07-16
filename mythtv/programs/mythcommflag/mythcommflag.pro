include ( ../../config.mak )
include (../../settings.pro)
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp CommDetectorFactory.cpp CommDetectorBase.cpp Histogram.cpp
SOURCES += ClassicCommDetector.cpp ClassicLogoDetector.cpp 
SOURCES += ClassicSceneChangeDetector.cpp

HEADERS += CommDetectorFactory.h SlotRelayer.h CustomEventRelayer.h
HEADERS += SceneChangeDetectorBase.h LogoDetectorBase.h
HEADERS += CommDetectorBase.h Histogram.h
HEADERS += ClassicCommDetector.h ClassicLogoDetector.h
HEADERS += ClassicSceneChangeDetector.h

