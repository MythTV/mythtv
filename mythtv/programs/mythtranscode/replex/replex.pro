include ( ../../../settings.pro)

TEMPLATE = app
TARGET = mythreplex
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += avi.c element.c mpg_common.c multiplex.c ringbuffer.c
SOURCES += ts.c replex.c pes.c

HEADERS += avi.h element.h mpg_common.h multiplex.h ringbuffer.h
HEADERS += ts.h replex.h pes.h

INCLUDEPATH += replex
INCLUDEPATH += ../../../external/FFmpeg
INCLUDEPATH += ../../../libs

QMAKE_CFLAGS += -Wno-unused-result -Wno-unused-function

LIBS += -L../../../external/FFmpeg/libavutil -L../../../external/FFmpeg/libavcodec -L../../../external/FFmpeg/libavcore -L../../../external/FFmpeg/libavformat
LIBS += -lmythavformat -lmythavcodec -lmythavcore -lmythavutil
LIBS += $$EXTRA_LIBS

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}

TARGETDEPS += ../../../external/FFmpeg/libavutil/$$avLibName(avutil)
TARGETDEPS += ../../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
TARGETDEPS += ../../../external/FFmpeg/libavcore/$$avLibName(avcore)
TARGETDEPS += ../../../external/FFmpeg/libavformat/$$avLibName(avformat)

DEPENDPATH += ../../../external/FFmpeg

CONFIG  -= qt
