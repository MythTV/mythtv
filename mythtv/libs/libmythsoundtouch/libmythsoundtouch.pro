include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythsoundtouch-$$LIBVERSION
CONFIG += thread staticlib warn_off
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.16.0 

include ( ../../config.mak )

!exists( ../../config.mak ) {
    error(Please run the configure script first)
}

INCLUDEPATH += ../../ 

# Input
HEADERS += AAFilter.h
HEADERS += cpu_detect.h
HEADERS += FIRFilter.h
HEADERS += RateTransposer.h
HEADERS += TDStretch.h

SOURCES += AAFilter.cpp
SOURCES += FIRFilter.cpp
SOURCES += FIFOSampleBuffer.cpp
SOURCES += RateTransposer.cpp
SOURCES += SoundTouch.cpp
SOURCES += TDStretch.cpp
SOURCES += cpu_detect_x86_gcc.cpp
SOURCES += mmx_gcc.cpp
