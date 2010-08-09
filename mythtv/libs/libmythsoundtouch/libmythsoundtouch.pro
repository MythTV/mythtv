include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythsoundtouch-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt

INCLUDEPATH += ../../libs/libavcodec ../..

# Like libavcodec, debug mode on x86 runs out of registers on some GCCs.
!profile:QMAKE_CXXFLAGS_DEBUG += -O

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

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

contains(ARCH_X86, yes) {
        DEFINES += ALLOW_SSE2 ALLOW_SSE3
        SOURCES += sse_gcc.cpp
}

include ( ../libs-targetfix.pro )
