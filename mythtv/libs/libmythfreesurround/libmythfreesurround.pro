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

DEPENDPATH += ../.. ../../external/FFmpeg
LIBS += -L../libavcodec -L../libavcore -L../libavutil
LIBS += -lavcodec -lavcore -lavutil

LIBS += $$EXTRA_LIBS

include ( ../libs-targetfix.pro )
