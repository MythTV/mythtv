include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythfreesurround-$$LIBVERSION
CONFIG += thread staticlib warn_off

INCLUDEPATH += ../.. ../../external/FFmpeg ../libmythbase .. ../..

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += el_processor.h
HEADERS += freesurround.h

SOURCES += el_processor.cpp
SOURCES += freesurround.cpp

contains( CONFIG_LIBFFTW3, yes ) {
    #required until its rewritten to use avcodec fft lib
    DEFINES += USE_FFTW3
} else {
    DEPENDPATH += ../.. ../../external/FFmpeg
    LIBS += -L../libavcodec
    LIBS += -L../libavcore
    LIBS += -L../libavutil
    LIBS += -lavcodec
    LIBS += -lavcore
    LIBS += -lavutil
}

LIBS += $$EXTRA_LIBS

include ( ../libs-targetfix.pro )
